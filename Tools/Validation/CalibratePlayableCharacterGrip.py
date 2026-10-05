"""UE-only contact-frame calibration. Produces a candidate; never saves assets.

Input: {schema_version:1, profiles:[{identity, weapon_tag, source_socket,
target_socket, hand_contact, weapon_contact, weapon_relative, anchor_scale}]}. Each transform
uses translation:[x,y,z], rotation:[x,y,z,w], scale:positive uniform scalar.
hand_contact is target socket local, weapon_contact is weapon actor root local,
weapon_relative is the equipped weapon root relative to its anchor (including
the spawn/equip scale rule). Values are local imported units, not world cm.
anchor_scale explicitly preserves the anchor's existing positive uniform scale;
the solver aligns contact position/rotation without resizing the weapon.
"""
import copy
import json
import math
import os
from pathlib import Path
import sys
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT/'Tools/Validation'))
from PlayableCharacterPolish import read_source, preflight, snapshot, equivalent, validate_appearance
from PlayableCharacterTransaction import write_json


def transform(row):
    if set(row) != {'translation', 'rotation', 'scale'}:
        raise ValueError('Transform requires translation, quaternion rotation and uniform scale')
    t, q, s = row['translation'], row['rotation'], row['scale']
    if len(t) != 3 or len(q) != 4 or not isinstance(s, (float, int)):
        raise ValueError('Invalid contact-frame dimensions; nonuniform scale is unsupported')
    if not all(math.isfinite(v) for v in [*t, *q, s]) or s <= 1e-6:
        raise ValueError('Invalid contact-frame values')
    if abs(sum(v*v for v in q)-1) > 1e-5:
        raise ValueError('Contact quaternion must be normalized')
    value = unreal.Transform()
    value.set_editor_property('translation', unreal.Vector(*t))
    value.set_editor_property('rotation', unreal.Quat(*q))
    value.set_editor_property('scale3d', unreal.Vector(s, s, s))
    return value


def compose(a, b):
    return unreal.MathLibrary.compose_transforms(a, b)


def residual(a, b):
    distance = math.sqrt(sum((getattr(a.translation,k)-getattr(b.translation,k))**2 for k in 'xyz'))
    dot = abs(sum(getattr(a.rotation,k)*getattr(b.rotation,k) for k in 'xyzw'))
    angle = math.degrees(2*math.acos(min(1., dot)))
    scale = max(abs(getattr(a.scale3d,k)-getattr(b.scale3d,k)) for k in 'xyz')
    return dict(local_position=distance, angle_degrees=angle, scale=scale)


def solve(hand, contact, relative, anchor_scale):
    # UE applies A then B: contact * relative * anchor = hand.
    combined = compose(contact, relative)
    desired = unreal.Transform()
    desired.set_editor_property('translation', hand.translation)
    desired.set_editor_property('rotation', hand.rotation)
    # Contact frames specify axes, not the physical size of the weapon.
    scale = transform(dict(translation=[0,0,0], rotation=[0,0,0,1], scale=anchor_scale)).scale3d
    desired.set_editor_property('scale3d', unreal.Vector(*(getattr(combined.scale3d,k)*getattr(scale,k) for k in 'xyz')))
    anchor = compose(unreal.MathLibrary.invert_transform(combined), desired)
    error = residual(compose(combined, anchor), desired)
    if error['local_position'] > 1e-4 or error['angle_degrees'] > .001 or error['scale'] > 1e-5:
        raise ValueError('Contact reconstruction failed: '+str(error))
    return anchor, error


def self_test():
    cases = []
    for scale in (.01, 1., 100.):
        frame = lambda t, q, s: transform(dict(translation=t, rotation=q, scale=s))
        rotation = [0, 0, math.sin(.3), math.cos(.3)]
        contact = frame([2, 3, 4], rotation, 1.)
        relative = frame([5, -6, 7], [math.sin(.2), 0, 0, math.cos(.2)], scale)
        expected = frame([-3, 8, 2], [0, math.sin(.4), 0, math.cos(.4)], .7)
        hand = compose(compose(contact, relative), expected)
        hand.set_editor_property('scale3d', unreal.Vector(1,1,1))
        actual, error = solve(hand, contact, relative, .7)
        delta = residual(actual, expected)
        if delta['local_position'] > 1e-4 or delta['angle_degrees'] > .001 or delta['scale'] > 1e-5:
            raise AssertionError(delta)
        cases.append(dict(scale=scale, residual=error))
    for bad in ([1, 2, 1], 0, -1, float('nan')):
        try: transform(dict(translation=[0,0,0], rotation=[0,0,0,1], scale=bad))
        except ValueError: pass
        else: raise AssertionError('Unsupported scale accepted')
    return cases


def main():
    output = Path(os.environ['PG_CHARACTER_RUN'])
    tests = self_test()
    input_path = os.environ.get('PG_GRIP_CALIBRATION')
    source = read_source()
    preflight(source)
    if input_path:
        data = json.loads(Path(input_path).read_text(encoding='utf-8-sig'))
    else:
        # Synthetic fixtures exercise reflected serialization and restoration on
        # all seven actual rigs. They must never become a production candidate.
        data = dict(schema_version=1, profiles=[])
        for identity in ['Bokusei','LianLian','Honoka','Hichi','Siuha','Lili','Nenmir']:
            target = source['assets']['/Game/DataCenter/Characters/DA_'+identity]['fields']['equipment_bones']['hand_r']
            frame = dict(translation=[1,2,3], rotation=[0,0,0,1], scale=1)
            data['profiles'].append(dict(identity=identity, weapon_tag='Weapon.Sword', source_socket='hand_r',
                target_socket=target, hand_contact=frame, weapon_contact=frame, weapon_relative=frame, anchor_scale=1))
    if data.get('schema_version') != 1 or not data.get('profiles'):
        raise ValueError('Expected schema 1 and at least one authored contact profile')
    candidate = copy.deepcopy(source)
    keys, observations = set(), []
    for row in data['profiles']:
        package = '/Game/DataCenter/Characters/DA_'+row['identity']
        if package not in candidate['assets']:
            raise ValueError('Unknown identity: '+row['identity'])
        key = (package.casefold(), row['weapon_tag'].casefold(), row['source_socket'].casefold())
        if key in keys: raise ValueError('Duplicate contact profile: '+str(key))
        keys.add(key)
        if row['weapon_tag'] != 'Weapon.Sword':
            raise ValueError('This calibration pilot supports the existing Weapon.Sword tag only')
        anchor, error = solve(*(transform(row[k]) for k in ('hand_contact','weapon_contact','weapon_relative')), row['anchor_scale'])
        grip = unreal.PGAppearanceGripProfile()
        tag = unreal.GameplayTag()
        # import_text uses UE's registered GameplayTag representation; validation
        # below also checks the source/target socket and duplicate equipment key.
        if not row['weapon_tag'].replace('.', '').replace('_', '').isalnum():
            raise ValueError('Invalid weapon tag spelling')
        tag.import_text('(TagName="'+row['weapon_tag']+'")')
        grip.set_editor_property('weapon_tag', tag)
        grip.set_editor_property('source_socket', row['source_socket'])
        grip.set_editor_property('target_socket', row['target_socket'])
        grip.set_editor_property('grip_offset', anchor)
        asset = unreal.load_asset(package)
        before = snapshot(asset)
        if not equivalent(before, source['assets'][package]):
            raise ValueError('Apply/export pending polish changes before calibrating '+package)
        original = list(asset.get_editor_property('grip_profiles'))
        if 'fingers' in row:
            fingers = []
            for entry in row['fingers']:
                finger = unreal.PGAppearanceGripFinger()
                finger.bone = entry['bone']
                pitch, yaw, roll = entry['reference_rotation_offset']
                finger.reference_rotation_offset = unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll)
                fingers.append(finger)
            grip.fingers = fingers
        else:
            # Recalibrating the contact must preserve an already authored hand pose.
            for existing in original:
                if str(unreal.GameplayTagLibrary.get_tag_name(existing.weapon_tag)).casefold() == key[1] and str(existing.source_socket).casefold() == key[2]:
                    grip.fingers = existing.fingers
        try:
            profiles = []
            for text in candidate['assets'][package]['fields']['grip_profiles']:
                existing = unreal.PGAppearanceGripProfile()
                existing.import_text(text)
                existing_key = (str(unreal.GameplayTagLibrary.get_tag_name(existing.weapon_tag)).casefold(), str(existing.source_socket).casefold())
                if existing_key != key[1:]: profiles.append(existing)
            asset.set_editor_property('grip_profiles', profiles+[grip])
            validate_appearance(asset)
            candidate['assets'][package]['fields']['grip_profiles'] = snapshot(asset)['fields']['grip_profiles']
        finally:
            asset.set_editor_property('grip_profiles', original)
            assert equivalent(before, snapshot(asset)), 'In-memory source restoration failed'
        observations.append(dict(identity=row['identity'], reconstruction=error, grip_profile=grip.export_text()))
    if input_path:
        write_json(output/'candidate.json', candidate)
        write_json(output/'calibration-input.json', data)
    write_json(output/'calibration.json', dict(status='CANDIDATE' if input_path else 'PASS', profiles=observations, tests=tests,
        synthetic=not bool(input_path),
        assets_saved=0, visual_acceptance=False, world_contact_acceptance=False))


main()
