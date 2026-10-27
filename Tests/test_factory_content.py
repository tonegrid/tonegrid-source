"""Extraction and conflict checks use synthetic temporary files, never user data."""
import hashlib, importlib.util, json, tempfile, unittest, zipfile
from pathlib import Path

spec=importlib.util.spec_from_file_location('factory',Path(__file__).resolve().parents[1]/'tools/factory-content.py')
factory=importlib.util.module_from_spec(spec);spec.loader.exec_module(factory)

class ContentTests(unittest.TestCase):
 def setUp(self):
  cache=Path(__file__).resolve().parents[1]/'.factory-content-cache';cache.mkdir(exist_ok=True)
  self.temp=tempfile.TemporaryDirectory(prefix='test-',dir=cache);self.root=Path(self.temp.name)/'checkout';self.root.mkdir();self.archive=Path(self.temp.name)/'content.zip'
  self.payload={'Resources/FactoryAmps/Test.nam':b'capture','Resources/FactoryCabinetIR/1x12/Pack/Test.wav':b'cabinet','Resources/ToneBank/tonebank.json':b'{\r\n"test":true\r\n}'}
  self.lock=dict(ampCaptures=1,pedalCaptures=0,cabinetIRs=1,recommendations=1,files=[dict(path=n,bytes=len(v),sha256=hashlib.sha256(v).hexdigest()) for n,v in self.payload.items()])
  self.write_archive()
 def tearDown(self):self.temp.cleanup()
 def write_archive(self,extra=None):
  with zipfile.ZipFile(self.archive,'w') as z:
   for n,v in self.payload.items():z.writestr(n,v)
   z.writestr('README.txt','fixture');z.writestr('FACTORY-CONTENT-MANIFEST.json',json.dumps(self.lock))
   if extra:z.writestr(*extra)
  self.lock['sha256']=factory.sha(self.archive)
 def test_roundtrip_and_idempotence(self):
  factory.install(self.archive,self.root,self.lock);factory.install(self.archive,self.root,self.lock)
  self.assertTrue(factory.verify_local(self.root,self.lock))
  for n,v in self.payload.items():self.assertEqual((self.root/n).read_bytes(),v)
 def test_conflict_does_not_partially_install(self):
  p=self.root/'Resources/ToneBank/tonebank.json';p.parent.mkdir(parents=True);p.write_bytes(b'personal edit')
  with self.assertRaises(ValueError):factory.install(self.archive,self.root,self.lock)
  self.assertEqual(p.read_bytes(),b'personal edit');self.assertFalse((self.root/'Resources/FactoryAmps/Test.nam').exists())
 def test_tampered_archive(self):
  self.archive.write_bytes(self.archive.read_bytes()+b'changed')
  with self.assertRaises(ValueError):factory.install(self.archive,self.root,self.lock)
  self.assertEqual(list(self.root.iterdir()),[])
 def test_extra_entry(self):
  self.write_archive(('Resources/FactoryAmps/Other.nam',b'unknown'))
  with self.assertRaises(ValueError):factory.install(self.archive,self.root,self.lock)
 def test_unexpected_local_audio_is_preserved(self):
  factory.install(self.archive,self.root,self.lock);p=self.root/'Resources/FactoryAmps/Other.nam';p.write_bytes(b'local')
  with self.assertRaises(ValueError):factory.verify_local(self.root,self.lock)
  self.assertEqual(p.read_bytes(),b'local')
 def test_paths(self):
  for n in ['../outside','/Resources/Models/x','Resources/FactoryAmps/../x','Resources//Models/x','Resources/Models/C:x','Resources/Unknown/x','Resources/Models\\x']:
   with self.subTest(n=n),self.assertRaises(ValueError):factory.local(self.root,n)
 def test_symlink_parent(self):
  parent=self.root/'Resources';parent.mkdir()
  try:(parent/'Models').symlink_to(self.root,target_is_directory=True)
  except OSError:self.skipTest('Symlink privilege unavailable on this test host')
  with self.assertRaises(ValueError):factory.local(self.root,'Resources/Models/file.nam')

if __name__=='__main__':unittest.main(verbosity=2)
