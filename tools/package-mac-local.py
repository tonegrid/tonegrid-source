"""Package a verified local/ad-hoc arm64 app, with no external readme or test data.

Run after tests and after moving their evidence out of Contents/MacOS.
This never notarizes, publishes, installs an app, or changes user data.
"""
import argparse,hashlib,json,plistlib,shutil,subprocess,tempfile,zipfile,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
VERSION=re.search(r'project\(ToneGrid VERSION ([0-9.]+)',(ROOT/'CMakeLists.txt').read_text()).group(1)
def digest(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for block in iter(lambda:f.read(1048576),b''):h.update(block)
 return h.hexdigest()
def run(*args):return subprocess.check_output(args,text=True,stderr=subprocess.STDOUT)
def verify(app,lock,signature=True):
 info=plistlib.loads((app/'Contents/Info.plist').read_bytes())
 assert info['CFBundleShortVersionString']==info['CFBundleVersion']==VERSION
 assert info['CFBundleExecutable']=='ToneGrid' and info['CFBundleName']=='ToneGrid'
 assert info['CFBundleIdentifier']=='com.personalaudiolab.tonexstateprobe'
 assert info.get('NSMicrophoneUsageDescription')
 assert info.get('LSMinimumSystemVersion')=='13.0'
 assert set(p.name for p in (app/'Contents/MacOS').iterdir())=={'ToneGrid','lame'},'Test output or unknown code in bundle'
 for name in ('ToneGrid','lame'):
  binary=app/'Contents/MacOS'/name
  assert run('lipo','-archs',str(binary)).strip()=='arm64'
  dependencies='\n'.join(run('otool','-L',str(binary)).splitlines()[1:])
  assert all(x not in dependencies for x in ('/opt/homebrew/','/usr/local/','/.codex/','/build-mac'))
 for item in lock['files']:
  p=app/'Contents'/item['path']
  assert p.stat().st_size==item['bytes'] and digest(p)==item['sha256'],str(p)
 expected={item['path'] for item in lock['files'] if item['path'].lower().endswith(('.nam','.wav','.mp3','.flac','.aiff'))}
 actual={p.relative_to(app/'Contents').as_posix() for p in app.rglob('*') if p.is_file() and p.suffix.lower() in ('.nam','.wav','.mp3','.flac','.aiff')}
 assert actual==expected,'Unexpected/missing factory audio or personal recording'
 for name in ('FactoryAmps','Models','FactoryCabinetIR','ToneBank','StarterPresets'):
  expectedFiles={i['path'] for i in lock['files'] if i['path'].startswith('Resources/'+name+'/')}
  actualFiles={p.relative_to(app/'Contents').as_posix() for p in (app/'Contents/Resources'/name).rglob('*') if p.is_file()}
  assert actualFiles==expectedFiles,'Unexpected payload in '+name
 for p in app.rglob('*'):
  assert p.name not in ('.DS_Store','WebView2','Recordings','CMakeCache.txt','.factory-content-cache'),str(p)
 for source,destination in (('PedalAssets/V137','PedalSkins'),('Jam','Jam'),('Learn','Learn'),('Calibration','Calibration'),('Translations','Translations')):
  for original in (ROOT/'Resources'/source).rglob('*'):
   if original.is_file():
    bundled=app/'Contents/Resources'/destination/original.relative_to(ROOT/'Resources'/source)
    assert bundled.is_file() and digest(original)==digest(bundled),str(bundled)
 for name in ('LICENSE.txt','LICENSE-NOTICE.txt'):
  assert digest(ROOT/name)==digest(app/'Contents/Resources'/name)
 assert (app/'Contents/Resources/Licenses/LAME-COPYING.txt').is_file()
 assert (app/'Contents/Resources/Tools/lame-3.100-source.tar.gz').is_file()
 if signature:run('codesign','--verify','--deep','--strict',str(app))
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('app',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 app=a.app.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 stem='MacOS.ToneGrid-'+VERSION.replace('.', '-');archive=out/(stem+'.zip');destination=out/('ToneGrid-'+VERSION+'-macOS-arm64');report=out/(stem+'.validation.json')
 assert not archive.exists() and not destination.exists(),'Preserve existing artifacts'
 lock=json.loads((ROOT/'Resources/factory-content-lock.json').read_text())
 subprocess.run(['python3',str(ROOT/'tools/factory-content.py'),'verify'],check=True)
 verify(app,lock,False);destination.mkdir();copy=destination/'ToneGrid.app'
 subprocess.run(['ditto',str(app),str(copy)],check=True)
 subprocess.run(['codesign','--force','--sign','-',str(copy/'Contents/MacOS/lame')],check=True)
 subprocess.run(['codesign','--force','--sign','-',str(copy)],check=True)
 verify(copy,lock)
 subprocess.run(['ditto','-c','-k','--norsrc','--keepParent',str(copy),str(archive)],check=True)
 with zipfile.ZipFile(archive) as z:
  assert z.testzip() is None
  assert all(n.split('/')[0]=='ToneGrid.app' for n in z.namelist())
 with tempfile.TemporaryDirectory(prefix='tonegrid-package-') as temporary:
  subprocess.run(['ditto','-x','-k',str(archive),temporary],check=True);verify(Path(temporary)/'ToneGrid.app',lock)
 sha=digest(archive);archive.with_suffix('.zip.sha256').write_text(sha+'  '+archive.name+'\n')
 result=dict(app=str(copy),zip=str(archive),sha256=sha,bytes=archive.stat().st_size,version=VERSION,architecture='arm64',signing='local ad-hoc; not notarized',sourceCommit=(run('git','-C',str(ROOT),'rev-parse','HEAD').strip() if (ROOT/'.git').exists() else (ROOT/'SOURCE-REVISION.txt').read_text(encoding='utf-8').strip()),factoryArchiveSHA256=lock['sha256'],factoryLockSHA256=digest(ROOT/'Resources/factory-content-lock.json'),factoryFiles=len(lock['files']),verifiedAfterExtraction=True)
 report.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
