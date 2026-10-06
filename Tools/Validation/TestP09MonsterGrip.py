"""Reject incomplete captures and invalid saved grip geometry."""
import copy
from pathlib import Path
import tempfile
import unittest
from RunP09MonsterGrip import check_evidence,expected_captures

class P09MonsterGripEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.out=Path(self.temp.name)
        self.expected=expected_captures()
        samples=[]
        for name in sorted(self.expected):
            eid,sid,fraction,_,side,view=Path(name).stem.split('_')
            samples.append(dict(enemy=int(eid),skill=int(sid),fraction=int(fraction)/100,
                bone='hand_'+side,view=view,contact_error_cm=0.,hand_inside_arm_bounds=True))
            (self.out/name).write_bytes(b'capture')
        self.poses=dict(status='CAPTURED',samples=samples)

    def check(self,poses=None,candidate=False):
        return check_evidence(self.out,poses or self.poses,self.expected,candidate)

    def test_complete_six_loadouts(self):
        self.assertEqual(len(self.expected),150)
        self.assertEqual(self.check(),[])
        self.assertEqual(len(expected_captures(15201)),12)
        self.assertEqual(len(expected_captures(15206)),42)

    def test_missing_or_empty_image_fails(self):
        image=self.out/next(iter(self.expected))
        image.unlink()
        self.assertTrue(self.check())
        image.write_bytes(b'')
        self.assertTrue(self.check())

    def test_missing_or_duplicate_pose_fails(self):
        poses=copy.deepcopy(self.poses)
        poses['samples'].pop()
        self.assertTrue(self.check(poses))
        poses['samples'].append(poses['samples'][0])
        self.assertTrue(self.check(poses))

    def test_nonfinite_or_excessive_contact_fails(self):
        for error in (float('nan'),float('inf'),.15,10.):
            with self.subTest(error=error):
                self.poses['samples'][0]['contact_error_cm']=error
                self.assertTrue(self.check())

    def test_out_of_bounds_or_unfinished_capture_fails(self):
        self.poses['samples'][0]['hand_inside_arm_bounds']=False
        self.assertTrue(self.check())
        self.poses['samples'][0]['hand_inside_arm_bounds']=True
        self.poses['status']='FAIL'
        self.assertTrue(self.check())

    def test_candidate_skips_geometry_only(self):
        self.poses['samples'][0]['contact_error_cm']=10.
        self.poses['samples'][0]['hand_inside_arm_bounds']=False
        self.assertEqual(self.check(candidate=True),[])
        (self.out/next(iter(self.expected))).unlink()
        self.assertTrue(self.check(candidate=True))

if __name__=='__main__':unittest.main()
