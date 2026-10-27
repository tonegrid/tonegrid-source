"""Build an UNSIGNED Inno review installer from an independently verified package.

No application launch, install, publication, signing request or profile writes.
Production/signing modes are intentionally kept as reviewed manual integration
steps until provider approval; this helper always compiles ReviewBuild=1.
"""
from pathlib import Path,PurePosixPath
import argparse,hashlib,json,subprocess,zipfile
ROOT=Path(__file__).resolve().parents[1]
def digest(data):return hashlib.sha256(data).hexdigest()
def js(value):return json.dumps(value,ensure_ascii=False,indent=2)+'\n'
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--package',type=Path,required=True);p.add_argument('--compiler',type=Path,required=True)
 p.add_argument('--work',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 p.add_argument('--upgrade-fixture',action='store_true',help='Test-only prior installer with two removable owned fixture files')
 a=p.parse_args();work=a.work.resolve();output=a.output.resolve()
 if work.exists():raise SystemExit('Use a fresh installer work directory')
 work.mkdir(parents=True);output.mkdir(parents=True,exist_ok=True);payload=work/'payload';payload.mkdir()
 with zipfile.ZipFile(a.package) as z:
  names=z.namelist();prefix='ToneGrid-1.1.44/';manifest=json.loads(z.read(prefix+'PACKAGE-MANIFEST.json'))
  expected={prefix+n for n in manifest['files']}|{prefix+'PACKAGE-MANIFEST.json'}
  if len(names)!=len(set(names)) or set(names)!=expected:raise ValueError('Unexpected package members')
  for name in names:
   relative=name.removeprefix(prefix);parts=PurePosixPath(relative).parts
   if not parts or any(x in ('.','..') for x in parts) or any(c in relative for c in '\\:"|\r\n') or relative.startswith('/'):raise ValueError('Unsafe package path')
   data=z.read(name)
   if relative in manifest['files']:
    item=manifest['files'][relative]
    if len(data)!=item['bytes'] or digest(data)!=item['sha256']:raise ValueError('Package checksum mismatch: '+relative)
   target=payload/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
 # Publish the app's actual privacy notice inside the installed program directory.
 (payload/'PRIVACY.md').write_bytes((ROOT/'PRIVACY.md').read_bytes())
 if a.upgrade_fixture:
  for name in ('retired-stock.txt','retired-modified.txt'):
   f=payload/'InstallerUpgradeFixture'/name;f.parent.mkdir(exist_ok=True);f.write_bytes(b'Known previous installer-owned synthetic file\n')
 files=sorted(f for f in payload.rglob('*') if f.is_file())
 lines=[];entries=[]
 for f in files:
  relative=f.relative_to(payload).as_posix();sha=digest(f.read_bytes());lines.append(sha+'|'+relative.replace('/','\\'))
  dest=PurePosixPath(relative).parent.as_posix();destination='{app}'+('\\'+dest.replace('/','\\') if dest!='.' else '')
  entries.append('Source: "{#PayloadDir}\\'+relative.replace('/','\\')+'"; DestDir: "'+destination+'"; Flags: ignoreversion')
 (payload/'ToneGrid-installed-files.sha256').write_text('\n'.join(lines)+'\n',encoding='utf-8-sig',newline='\n')
 include=work/'payload-files.iss';include.write_text('\n'.join(entries)+'\n',encoding='utf-8-sig',newline='\n')
 stem='ToneGrid-1.1.44-Windows-x64-Unsigned-'+('UpgradeFixture' if a.upgrade_fixture else 'Review')
 artifact=output/(stem+'.exe')
 if artifact.exists():raise SystemExit('Refusing to overwrite existing installer')
 args=[str(a.compiler.resolve()),'/DReviewBuild=1','/DInstallerRevision='+('0' if a.upgrade_fixture else '1'),'/DPayloadDir='+str(payload),'/DPayloadList='+str(include),'/DOutputDir='+str(output),'/DOutputStem='+stem,str(ROOT/'installer/windows/ToneGrid.iss')]
 with (work/'compiler.log').open('w',encoding='utf-8') as log:r=subprocess.run(args,stdout=log,stderr=subprocess.STDOUT)
 if r.returncode:raise SystemExit('Inno compilation failed; inspect compiler.log')
 result=dict(installer=artifact.name,sha256=digest(artifact.read_bytes()),bytes=artifact.stat().st_size,packageSHA256=digest(a.package.read_bytes()),packageSourceCommit=manifest['sourceCommit'],compilerSHA256=digest(a.compiler.read_bytes()),reviewOnly=True,signing='UNSIGNED; no signer configured; SignedUninstaller=no',testFixture=a.upgrade_fixture,payloadFiles=len(files)+1)
 artifact.with_suffix('.manifest.json').write_text(js(result),encoding='utf-8',newline='\n')
 artifact.with_suffix('.exe.sha256').write_text(result['sha256']+'  '+artifact.name+'\n',encoding='utf-8',newline='\n')
 print(js(result))
if __name__=='__main__':main()
