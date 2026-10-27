"""Check application catalogue requirements against the public resource lock.

This independent closure check needs only shipped metadata, not personal data.
It catches files omitted from an otherwise self-consistent download inventory.
"""
from pathlib import Path
import json,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
def main():
 lock=json.loads((ROOT/'Resources/public-content-lock.json').read_bytes())
 inventory={item['path']:item for item in lock['files']}
 if len(inventory)!=len(lock['files']):raise ValueError('Duplicate public resource path')
 checks=0
 def require(path,row):
  nonlocal checks
  if path not in inventory:raise ValueError('Runtime catalogue requires omitted resource: '+path)
  item=inventory[path]
  if item['sha256']!=row['sha256'] or ('bytes' in row and item['bytes']!=row['bytes']):raise ValueError('Catalogue/lock identity mismatch: '+path)
  checks+=1
 amps=json.loads((ROOT/'Resources/FactoryAmps/manifest.json').read_bytes())
 for item in amps['amps']+amps.get('assets',[]):require('Resources/FactoryAmps/'+item['filename'],item)
 cabs=json.loads((ROOT/'Resources/FactoryCabinetIR/catalog.json').read_bytes())
 for item in cabs['irs']+cabs.get('assets',[]):require('Resources/FactoryCabinetIR/'+item['file'],item)
 pedals=ET.parse(ROOT/'Resources/Models/catalog.xml').getroot()
 for group in pedals:
  for item in group:require('Resources/Models/'+item.attrib['file'],item.attrib)
 starters=json.loads((ROOT/'Resources/StarterPresets/manifest.json').read_bytes())
 for item in starters['presets']:require('Resources/StarterPresets/'+item['file'],item)
 print(json.dumps(dict(requiredCatalogueMembers=checks,lockedResourceFiles=len(inventory),catalogueClosureVerified=True),indent=2))
if __name__=='__main__':main()
