"""Unit tests for import-brand.py. Needs Pillow. Run with the other library tests:
python -m unittest discover -s branding/library -p "test_*.py" -v
"""
import importlib.util
import pathlib
import tempfile
import unittest

from PIL import Image, ImageDraw

HERE = pathlib.Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("import_brand", HERE / "import-brand.py")
ib = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ib)


class ImportBrand(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = pathlib.Path(self.tmp.name)
        self.library = self.dir / "library"
        (self.library / "brands").mkdir(parents=True)
        self.brands_ini = self.dir / "brands.ini"

    def tearDown(self):
        self.tmp.cleanup()

    def art(self, size, colour=(200, 30, 40, 255), mode="RGBA", round_logo=False):
        width, height = size if isinstance(size, tuple) else (size, size)
        if round_logo:
            image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
            ImageDraw.Draw(image).ellipse((50, 50, width - 51, height - 51), fill=colour)
        else:
            image = Image.new(mode, (width, height), colour if mode == "RGBA" else colour[:3])
        path = self.dir / f"src-{width}x{height}-{mode}-{round_logo}.png"
        image.save(path)
        return path

    def run_import(self, name, path, **kwargs):
        return ib.import_brand(name, path, "https://example.com/listing", library=self.library,
                               brands_ini=self.brands_ini, **kwargs)

    def test_full_bleed_art_is_cut_to_the_outline(self):
        info = self.run_import("netflix", self.art(600))
        self.assertEqual(info["size"], 600)
        with Image.open(self.library / "brands" / "netflix.png") as out:
            self.assertEqual(out.mode, "RGBA")
            self.assertEqual(out.size, (600, 600))
            self.assertEqual(out.getpixel((0, 0))[3], 0)        # corner: outside the outline
            self.assertEqual(out.getpixel((300, 300))[3], 255)  # centre: inside
            self.assertEqual(out.getpixel((300, 300))[:3], (200, 30, 40))

    def test_brands_ini_records_provenance_and_keeps_hand_edits(self):
        self.brands_ini.write_text("[netflix]\ntitle = Netflix\ngroup = video\nowner = Netflix, Inc.\n", encoding="utf-8")
        info = self.run_import("netflix", self.art(512), art="https://example.com/art.png")
        keys = dict(ib.libtools.read_sections(self.brands_ini))["netflix"]
        self.assertEqual(keys["title"], "Netflix")
        self.assertEqual(keys["owner"], "Netflix, Inc.")
        self.assertEqual(keys["source"], "https://example.com/listing")
        self.assertEqual(keys["art"], "https://example.com/art.png")
        self.assertEqual(keys["size"], "512")
        self.assertEqual(keys["sha256"], info["sha256"])
        self.assertEqual(keys["sha256"], ib.libtools.sha256_file(self.library / "brands" / "netflix.png"))

    def test_big_art_is_reduced_to_1024(self):
        self.assertEqual(self.run_import("big", self.art(2048))["size"], 1024)

    def test_small_art_is_refused_never_enlarged(self):
        with self.assertRaises(ib.ImportRefused):
            self.run_import("small", self.art(300))
        self.assertFalse((self.library / "brands" / "small.png").exists())

    def test_non_square_art_is_refused(self):
        with self.assertRaises(ib.ImportRefused):
            self.run_import("wide", self.art((600, 500)))

    def test_rgb_art_is_accepted(self):
        self.assertEqual(self.run_import("rgb", self.art(512, mode="RGB"))["size"], 512)

    def test_transparent_art_needs_a_fill(self):
        path = self.art(600, round_logo=True)
        with self.assertRaises(ib.ImportRefused):
            self.run_import("round", path)
        self.run_import("round", path, fill="#112233")
        with Image.open(self.library / "brands" / "round.png") as out:
            self.assertEqual(out.getpixel((300, 20)), (17, 34, 51, 255))  # inside the outline, outside the logo
        self.assertEqual(dict(ib.libtools.read_sections(self.brands_ini))["round"]["fill"], "#112233")

    def test_near_opaque_art_counts_as_opaque(self):
        # Store art often carries alpha 254 from an encoder: not transparency, and not worth a --fill
        self.run_import("near", self.art(512, colour=(10, 120, 200, 252)))
        with Image.open(self.library / "brands" / "near.png") as out:
            self.assertEqual(out.getpixel((256, 256)), (10, 120, 200, 255))
            self.assertEqual(out.getpixel((0, 0))[3], 0)
        self.assertNotIn("fill", dict(ib.libtools.read_sections(self.brands_ini))["near"])

    def test_a_malformed_fill_is_refused_not_a_traceback(self):
        path = self.art(600, round_logo=True)
        for fill in ("#abc", "112233", "#GG0000", "#1122334"):
            with self.assertRaises(ib.ImportRefused, msg=fill):
                self.run_import("round", path, fill=fill)
        self.assertFalse((self.library / "brands" / "round.png").exists())

    def test_invalid_name_is_refused(self):
        with self.assertRaises(ib.ImportRefused):
            self.run_import("Net_Flix", self.art(512))


if __name__ == "__main__":
    unittest.main()
