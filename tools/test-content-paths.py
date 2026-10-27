"""Portable path guards and deterministic readable capture naming."""
import importlib.util
from pathlib import Path
import unittest
from content_paths import validate_runtime_path, validate_inventory, windows_units

spec=importlib.util.spec_from_file_location('short_names',Path(__file__).with_name('shorten-factory-amp-names.py'))
names=importlib.util.module_from_spec(spec);spec.loader.exec_module(names)


class PortablePaths(unittest.TestCase):
    def test_extraction_directory_has_space(self):
        self.assertLess(validate_inventory(['FactoryAmps/05 - High Gain/Hiwatt/Hiwatt - Bovine II - Off - Blend 2.nam'],'1.1.44')+90+1,260)

    def test_reject_long_physical_capture_name(self):
        with self.assertRaisesRegex(ValueError,'shorter physical filename'):
            validate_runtime_path('FactoryAmps/Hiwatt/'+'a'*65+'.nam','1.1.44')

    def test_reject_deep_delivery_path(self):
        with self.assertRaisesRegex(ValueError,'Delivery path too long'):
            validate_runtime_path('a'*80+'/'+'b'*80+'/file.json','1.1.44')

    def test_case_insensitive_archive_collision(self):
        with self.assertRaisesRegex(ValueError,'collision'):
            validate_inventory(['FactoryAmps/A.nam','FactoryAmps/a.nam'],'1.1.44')

    def test_count_windows_utf16_not_python_codepoints(self):
        self.assertEqual(windows_units('\U0001f3b8'),2)
        self.assertEqual(windows_units('Hiwatt'),6)

    def test_keep_hiwatt_boost_and_blend_distinct(self):
        stem='Hiwatt Super-Hi 50 - High Gain - Bovine Beheading II - Gain 6.5 Boost OFF - B6.5 M2.2 T3.5 - Presence 5.2 Depth II Master 4 - Blend 2'
        self.assertEqual(names.compact(stem),'Hiwatt - Bovine II - Off - Blend 2')
        self.assertNotEqual(names.compact(stem),names.compact(stem.replace('Boost OFF','Boost ON')))
        self.assertNotEqual(names.compact(stem),names.compact(stem.replace('Blend 2','Blend 3')))

    def test_orange_voltage_and_cabinet_stay_identifiable(self):
        stem='Orange OR60 - Crunch - Sweet Delivery - Gain 4 - Low Voltage - Marshall 1960BV 4x12 - Blend 2 (SM57 + SM58 boosted 4dB)'
        self.assertEqual(names.compact(stem),'OR60 - Sweet Delivery - G4 LV - M1960BV - B2')

    def test_fallback_collision_suffix_is_stable_and_bounded(self):
        a=names.fit('An especially long readable capture name',30,'first original')
        b=names.fit('An especially long readable capture name',30,'second original')
        self.assertLessEqual(windows_units(a),30);self.assertNotEqual(a,b)
        self.assertEqual(a,names.fit('An especially long readable capture name',30,'first original'))

    def test_reference_rewrite_preserves_historical_names(self):
        value={'amp':'old/path.nam','resource':{'filename':'old/path.nam'},'originalFilename':'old/path.nam'}
        self.assertEqual(names.rewrite(value,{'old/path.nam':'new/path.nam'}),{'amp':'new/path.nam','resource':{'filename':'new/path.nam'},'originalFilename':'old/path.nam'})


if __name__=='__main__':unittest.main()
