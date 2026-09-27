"""Unit tests for check-library.py's brand and listing checks. Needs Pillow."""
import importlib.util
import pathlib
import shutil
import tempfile
import unittest

from PIL import Image

HERE = pathlib.Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("check_library", HERE / "check-library.py")
cl = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(cl)


class CheckLibrary(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.library = pathlib.Path(self.tmp.name) / "library"
        shutil.copytree(cl.libtools.LIBRARY, self.library)

    def tearDown(self):
        self.tmp.cleanup()

    def test_the_real_library_passes(self):
        self.assertEqual(cl.check_files(self.library), [])

    def test_an_unlisted_file_is_reported(self):
        (self.library / "generic" / "stray.svg").write_text("<svg/>", encoding="utf-8")
        self.assertTrue(any("stray.svg is not listed" in p for p in cl.check_files(self.library)))

    def test_a_non_square_brand_is_reported(self):
        brand = next((self.library / "brands").glob("*.png"))
        Image.new("RGBA", (600, 500), (0, 0, 0, 0)).save(brand)
        problems = cl.check_files(self.library)
        self.assertTrue(any(brand.name in p and "square" in p for p in problems), problems)

    def test_a_brand_opaque_in_its_corner_is_reported(self):
        brand = next((self.library / "brands").glob("*.png"))
        Image.new("RGBA", (512, 512), (10, 20, 30, 255)).save(brand)
        problems = cl.check_files(self.library)
        self.assertTrue(any(brand.name in p and "outside the outline" in p for p in problems), problems)


if __name__ == "__main__":
    unittest.main()
