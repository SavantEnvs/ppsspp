// Known-answer checks for the code the harnesses fuzz, built with the project's normal flags
// (no sanitizers) and run by mayhem/test.sh. Prints one PASS/FAIL line per check and a summary.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <zlib.h>

#include "Common/Data/Format/IniFile.h"
#include "Common/Data/Format/JSONReader.h"
#include "Common/Data/Format/PNGLoad.h"
#include "Common/Data/Format/ZIMLoad.h"
#include "Common/Math/expression_parser.h"
#include "Core/ELF/ParamSFO.h"
#include "ext/at3_standalone/at3_decoders.h"
#include "mayhem/support/fake_expr_funcs.h"

static int g_pass = 0, g_fail = 0;

static void Check(const char *name, bool ok) {
	printf("%s %s\n", ok ? "PASS" : "FAIL", name);
	ok ? g_pass++ : g_fail++;
}

static bool Eval(const char *expr, uint32_t *out) {
	FakeFunctions f;
	return parseExpression(expr, &f, *out);
}

static void TestExpression() {
	struct { const char *expr; uint32_t want; } cases[] = {
		{"1+2*3", 7},
		{"(1+2)*3", 9},
		{"0x1000 + 0x20", 0x1020},
		{"10 - 3 - 2", 11},  // numbers are hex by default
		{"1 << 4 | 3", 19},
		{"7 & 3 ^ 1", 2},
		{"5 > 3", 1},
		{"5 < 3", 0},
		{"1 ? 11 : 22", 0x11},
		{"0 ? 11 : 22", 0x22},
		{"a0", 4 * 0x01010101u + 0x1234u},
		{"a0 + 4", 4 * 0x01010101u + 0x1234u + 4},
		{"sym2", 0x08800008},
	};
	for (const auto &c : cases) {
		uint32_t got = 0;
		bool ok = Eval(c.expr, &got) && got == c.want;
		std::string name = std::string("expression: ") + c.expr;
		Check(name.c_str(), ok);
	}
	uint32_t got = 0;
	Check("expression: unknown token is rejected", !Eval("nosuchthing + 1", &got));
	Check("expression: unbalanced paren is rejected", !Eval("(1+2", &got));
}

static void TestIni() {
	std::istringstream in("# comment\n[General]\nName = hello world\nCount = 42\nRatio = 1.5\nFlag = True\nBig = 4294967295\n\n[Other]\nKey=Val\n");
	IniFile ini;
	Check("ini: Load", ini.Load(in));
	std::string s;
	int i = 0;
	bool b = false;
	double d = 0;
	uint32_t u = 0;
	const Section *gen = ini.GetSection("General");
	Check("ini: section exists", gen != nullptr);
	if (!gen) return;
	Check("ini: string value", gen->Get("Name", &s) && s == "hello world");
	Check("ini: int value", gen->Get("Count", &i) && i == 42);
	Check("ini: double value", gen->Get("Ratio", &d) && d == 1.5);
	Check("ini: bool value", gen->Get("Flag", &b) && b);
	Check("ini: uint32 value", gen->Get("Big", &u) && u == 4294967295u);
	Check("ini: missing key", !gen->Get("Nope", &s));
	Check("ini: second section", ini.GetSection("Other") && ini.GetSection("Other")->Get("Key", &s) && s == "Val");
	std::vector<std::string> keys;
	Check("ini: key list", gen->GetKeys(&keys) && keys.size() == 5 && keys[0] == "Name");
}

static void TestJson() {
	const char *text = "{\"a\":[1,2.5,{\"b\":\"x\"}],\"n\":-3,\"t\":true,\"s\":[\"p\",\"q\"],\"name\":\"zed\"}";
	json::JsonReader r(text, strlen(text));
	Check("json: parses", r.ok());
	json::JsonGet root = r.root();
	Check("json: child count", root.numChildren() == 5);
	Check("json: int", root.getInt("n") == -3);
	Check("json: bool", root.getBool("t"));
	Check("json: string", strcmp(root.getStringOr("name", ""), "zed") == 0);
	const JsonNode *arr = root.getArray("a");
	Check("json: array present", arr != nullptr);
	if (arr) {
		int n = 0;
		double sum = 0;
		for (const auto i : arr->value) {
			n++;
			if (i->value.getTag() == JSON_NUMBER) sum += i->value.toNumber();
		}
		Check("json: array walk", n == 3 && sum == 3.5);
	}
	Check("json: bad input rejected", !json::JsonReader("{\"a\": [1, 2", 11).ok());
}

static void TestSfo() {
	ParamSFOData out;
	out.SetValue("TITLE", "Test Title", 128);
	out.SetValue("DISC_ID", "ULUS10000", 16);
	out.SetValue("PARENTAL_LEVEL", 5, 4);
	u8 *buf = nullptr;
	size_t size = 0;
	out.WriteSFO(&buf, &size);
	Check("sfo: written", buf != nullptr && size > 0x14);
	Check("sfo: magic", buf && memcmp(buf, "\0PSF", 4) == 0);
	ParamSFOData in;
	Check("sfo: read back", buf && in.ReadSFO(buf, size));
	Check("sfo: title", in.GetValueString("TITLE") == "Test Title");
	Check("sfo: disc id", in.GetValueString("DISC_ID") == "ULUS10000" && in.GetDiscID() == "ULUS10000");
	Check("sfo: int value", in.GetValueInt("PARENTAL_LEVEL") == 5);
	Check("sfo: key count", in.GetKeys().size() == 3);
	uint8_t junk[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	ParamSFOData bad;
	Check("sfo: junk rejected", !bad.ReadSFO(junk, sizeof(junk)));
	delete[] buf;
}

static const unsigned char kPngRgba[] = {137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,2,8,6,0,0,0,114,182,13,36,0,0,0,23,73,68,65,84,120,156,99,248,207,192,240,31,8,27,24,128,244,127,70,38,102,6,0,58,236,5,130,125,65,67,132,0,0,0,0,73,69,78,68,174,66,96,130};
static const unsigned char kPngIndexed[] = {137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,4,0,0,0,2,8,3,0,0,0,72,118,141,81,0,0,0,12,80,76,84,69,0,0,0,255,0,0,0,255,0,0,0,255,155,192,19,220,0,0,0,4,116,82,78,83,0,128,255,255,236,128,111,229,0,0,0,18,73,68,65,84,120,156,99,96,96,100,98,102,96,102,98,100,0,0,0,70,0,13,94,114,139,135,0,0,0,0,73,69,78,68,174,66,96,130};

static void TestPng() {
	int w = 0, h = 0;
	unsigned char *img = nullptr;
	Check("png: rgba load", pngLoadPtr(kPngRgba, sizeof(kPngRgba), &w, &h, &img) == 1);
	Check("png: rgba size", w == 2 && h == 2);
	static const unsigned char want[16] = {255, 0, 0, 255, 0, 255, 0, 128, 0, 0, 255, 255, 1, 2, 3, 0};
	Check("png: rgba pixels", img && memcmp(img, want, 16) == 0);
	free(img);
	img = nullptr;
	Check("png: indexed load", pngLoadIndexed(kPngIndexed, sizeof(kPngIndexed), &w, &h, &img) == 1);
	static const unsigned char widx[8] = {0, 1, 2, 3, 3, 2, 1, 0};
	Check("png: indexed pixels", w == 4 && h == 2 && img && memcmp(img, widx, 8) == 0);
	free(img);
	img = nullptr;
	Check("png: truncated rejected", pngLoadPtr(kPngRgba, 20, &w, &h, &img) == 0);
	free(img);
}

static void TestZim() {
	int width[ZIM_MAX_MIP_LEVELS] = {}, height[ZIM_MAX_MIP_LEVELS] = {};
	uint8_t *image[ZIM_MAX_MIP_LEVELS] = {};
	int flags = 0;
	uint8_t raw[16];
	for (int i = 0; i < 16; i++) raw[i] = (uint8_t)(i * 37);
	std::vector<uint8_t> zim = {'Z', 'I', 'M', 'G', 2, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0};
	zim.insert(zim.end(), raw, raw + 16);
	int levels = LoadZIMPtr(zim.data(), zim.size(), width, height, &flags, image);
	Check("zim: raw load", levels == 1 && width[0] == 2 && height[0] == 2);
	Check("zim: raw pixels", levels == 1 && memcmp(image[0], raw, 16) == 0);
	if (levels) free(image[0]);

	// zlib-compressed 4x4 RGBA with a full mip chain: 4x4, 2x2, 1x1.
	uint8_t mips[64 + 16 + 4];
	for (size_t i = 0; i < sizeof(mips); i++) mips[i] = (uint8_t)(i * 5);
	uLongf clen = compressBound(sizeof(mips));
	std::vector<uint8_t> comp(clen);
	compress(comp.data(), &clen, mips, sizeof(mips));
	std::vector<uint8_t> z2 = {'Z', 'I', 'M', 'G', 4, 0, 0, 0, 4, 0, 0, 0, 16 | 0, 1, 0, 0};
	z2.insert(z2.end(), comp.begin(), comp.begin() + clen);
	memset(image, 0, sizeof(image));
	levels = LoadZIMPtr(z2.data(), z2.size(), width, height, &flags, image);
	Check("zim: zlib mip levels", levels == 3 && width[1] == 2 && height[2] == 1);
	Check("zim: zlib pixels", levels == 3 && memcmp(image[0], mips, sizeof(mips)) == 0 && image[2][3] == mips[64 + 16 + 3]);
	if (levels) free(image[0]);

	memset(image, 0, sizeof(image));
	zim.pop_back();
	Check("zim: short payload rejected", LoadZIMPtr(zim.data(), zim.size(), width, height, &flags, image) == 0);
}

static void TestAtrac() {
	int blockAlign = 96;
	uint8_t extra[14] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0};
	ATRAC3Context *ctx = atrac3_alloc(1, &blockAlign, extra, sizeof(extra));
	Check("atrac3: alloc", ctx != nullptr && blockAlign == 96);
	if (ctx) {
		static float l[4096], r[4096];
		float *out[2] = {l, r};
		uint8_t frame[96] = {};
		int nb = -1;
		int res = atrac3_decode_frame(ctx, out, &nb, frame, sizeof(frame));
		// A silent frame is not a valid stream: the decoder reports AVERROR_INVALIDDATA after sizing the output.
		Check("atrac3: decode result", res == -1094995529 && nb == 1024);
		atrac3_free(ctx);
	}
	int badAlign = 100;
	Check("atrac3: bad block align rejected", atrac3_alloc(1, &badAlign, extra, sizeof(extra)) == nullptr);
	int bAlign = 0;
	Check("atrac3: missing block align rejected", atrac3_alloc(1, &bAlign, extra, sizeof(extra)) == nullptr);

	int align3p = 0;
	ATRAC3PContext *p = atrac3p_alloc(2, &align3p);
	Check("atrac3+: default block align", p != nullptr && align3p == 0x2e8);
	if (p) atrac3p_free(p);
}

int main() {
	TestExpression();
	TestIni();
	TestJson();
	TestSfo();
	TestPng();
	TestZim();
	TestAtrac();
	printf("SELFTEST-SUMMARY passed=%d failed=%d\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
