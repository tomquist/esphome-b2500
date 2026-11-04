#include "kv_message.h"

#include "test_util.h"

using namespace esphome::hame_device;

static void test_parse() {
  KvMap m;
  EXPECT(parse_kv("p1=1,p2=0,w1=124,e1=0:0,fc=202310231502", m));
  EXPECT_EQ(m.size(), 5u);
  EXPECT_STR(m["e1"], "0:0");
  EXPECT_STR(m["fc"], "202310231502");
  KvMap n;
  EXPECT(parse_kv(" a = 1 ,,b=,noeq,c=x=y\r\n", n));
  EXPECT_STR(n["a"], "1");
  EXPECT_STR(n["b"], "");
  EXPECT_STR(n["c"], "x=y");  // first '=' splits
  EXPECT_EQ(n.count("noeq"), 0u);
  KvMap e;
  EXPECT(!parse_kv("", e));
  EXPECT(!parse_kv("garbage", e));
}

static void test_template() {
  KvMap cur{{"a1", "00:00"}, {"md", "1"}};
  KeyLookup lookup = [&](const std::string &k) -> const std::string * {
    auto it = cur.find(k);
    return it == cur.end() ? nullptr : &it->second;
  };
  std::string out, missing;
  EXPECT(render_template("cd=4,md={value}", "0", lookup, out));
  EXPECT_STR(out, "cd=4,md=0");
  EXPECT(render_template("cd=20,md={md},a1={a1},v1={value}", "300", lookup, out));
  EXPECT_STR(out, "cd=20,md=1,a1=00:00,v1=300");
  EXPECT(!render_template("cd=20,b1={b1}", "", lookup, out, &missing));
  EXPECT_STR(missing, "b1");
  EXPECT(render_template("cd=20,b1={b1|23:59}", "", lookup, out));
  EXPECT_STR(out, "cd=20,b1=23:59");
  EXPECT(render_template("{{literal}} {value}", "x", lookup, out));
  EXPECT_STR(out, "{literal} x");
  EXPECT(render_template("cd=1", "", nullptr, out));
  EXPECT_STR(out, "cd=1");
}

static void test_fields() {
  KvMap m{{"w1", "124"}, {"w2", "118"}, {"l0", "45"}, {"st", "65535"}, {"x", "abc"}};
  double v;
  NumberField one;
  one.add_key("w1");
  EXPECT(one.extract(m, v) && v == 124);
  NumberField sum;
  sum.add_key("w1");
  sum.add_key("w2");
  EXPECT(sum.extract(m, v) && v == 242);
  NumberField missing;
  missing.add_key("w1");
  missing.add_key("w3");
  EXPECT(!missing.extract(m, v));
  EXPECT(missing.touches(m));
  NumberField bits;  // 45 = 0b101101: bits 2..3 = 0b11 = 3
  bits.add_key("l0");
  bits.set_bit(2, 2);
  EXPECT(bits.extract(m, v) && v == 3);
  NumberField sentinel;
  sentinel.add_key("st");
  sentinel.set_valid_max(59999);
  EXPECT(!sentinel.extract(m, v));
  NumberField scaled;
  scaled.add_key("w1");
  scaled.set_multiply(0.1);
  EXPECT(scaled.extract(m, v) && v > 12.39 && v < 12.41);
  NumberField text;
  text.add_key("x");
  EXPECT(!text.extract(m, v));
  KvMap other{{"zz", "1"}};
  EXPECT(!one.touches(other));
}

static void test_aggregates() {
  KvMap cells{{"a0", "3300"}, {"a1", "3310"}, {"a2", "0"}, {"a3", "3290"}};
  double v;
  NumberField f;
  for (auto k : {"a0", "a1", "a2", "a3", "a4"})
    f.add_key(k);
  f.set_aggregate(NumberField::MIN, true);
  EXPECT(f.extract(cells, v) && v == 3290);
  f.set_aggregate(NumberField::MAX, true);
  EXPECT(f.extract(cells, v) && v == 3310);
  f.set_aggregate(NumberField::SPREAD, true);
  EXPECT(f.extract(cells, v) && v == 20);
  f.set_aggregate(NumberField::AVERAGE, true);
  EXPECT(f.extract(cells, v) && v == 3300);
  f.set_aggregate(NumberField::AVERAGE, false);  // zero counts
  EXPECT(f.extract(cells, v) && v == 2475);
  f.set_aggregate(NumberField::SUM, false);  // a4 missing: no value
  EXPECT(!f.extract(cells, v));
  KvMap none{{"zz", "1"}};
  f.set_aggregate(NumberField::MIN, true);
  EXPECT(!f.extract(none, v));
}

static void test_numbers() {
  double v;
  EXPECT(parse_number("-12.5", v) && v == -12.5);
  EXPECT(!parse_number("12a", v));
  EXPECT(!parse_number("", v));
  EXPECT_STR(format_number(300), "300");
  EXPECT_STR(format_number(-3), "-3");
  EXPECT_STR(format_number(2.5), "2.5");
}

static void test_sections_and_rules() {
  KvMap cur{{"d1", "1"}, {"e1", "0:0"}, {"f1", "23:59"}, {"h1", "800"}};
  KeyLookup lookup = [&](const std::string &k) -> const std::string * {
    auto it = cur.find(k);
    return it == cur.end() ? nullptr : &it->second;
  };
  std::string out, missing;
  // Period 2 is not reported: its optional section drops out.
  EXPECT(render_template("cd=20,md=0,a1={value},b1={e1},e1={f1},v1={h1}[,a2={d2},b2={e2},e2={f2},v2={h2}]", "0",
                         lookup, out));
  EXPECT_STR(out, "cd=20,md=0,a1=0,b1=0:0,e1=23:59,v1=800");
  cur["d2"] = "0";
  cur["e2"] = "8:5";
  cur["f2"] = "9:0";
  cur["h2"] = "100";
  EXPECT(render_template("a1={d1}[,a2={d2},b2={e2},e2={f2},v2={value}]", "300", lookup, out));
  EXPECT_STR(out, "a1=1,a2=0,b2=8:5,e2=9:0,v2=300");
  EXPECT(render_template("[[x]] {{y}}", "", lookup, out));
  EXPECT_STR(out, "[x] {y}");
  // A required key outside any section still refuses.
  EXPECT(!render_template("a3={d3}[,b1={e1}]", "", lookup, out, &missing));
  EXPECT_STR(missing, "d3");

  MessageRule runtime;
  runtime.required = {"pe", "kn"};
  MessageRule ct;
  ct.required = {"dchrg_t", "seq_s"};
  ct.excluded = {"pe"};
  MessageRule calib;
  calib.required = {"cf", "df"};
  calib.key_count = 2;
  KvMap r{{"pe", "50"}, {"kn", "1000"}, {"x", "1"}};
  KvMap c{{"dchrg_t", "1"}, {"seq_s", "3"}, {"c0", "10"}};
  KvMap both{{"dchrg_t", "1"}, {"seq_s", "3"}, {"pe", "1"}};
  KvMap cal{{"cf", "1"}, {"df", "2"}};
  KvMap cal3{{"cf", "1"}, {"df", "2"}, {"x", "3"}};
  EXPECT(runtime.matches(r));
  EXPECT(!runtime.matches(c));
  EXPECT(ct.matches(c));
  EXPECT(!ct.matches(both));
  EXPECT(calib.matches(cal));
  EXPECT(!calib.matches(cal3));
}

static void test_time() {
  std::string out;
  EXPECT(pad_time("8:5", out));
  EXPECT_STR(out, "08:05");
  EXPECT(pad_time("24:0", out));
  EXPECT_STR(out, "24:00");
  EXPECT(!pad_time("8", out));
  EXPECT(!pad_time("a:b", out));
  EXPECT(unpad_time("08:05", out));
  EXPECT_STR(out, "8:5");
  EXPECT(unpad_time("23:59", out));
  EXPECT_STR(out, "23:59");
  EXPECT(!unpad_time("24:00", out));
  EXPECT(!unpad_time("12:60", out));
  EXPECT(!unpad_time("12:3:4", out));
}

int main() {
  test_sections_and_rules();
  test_time();
  test_parse();
  test_template();
  test_fields();
  test_numbers();
  test_aggregates();
  return g_failures ? 1 : 0;
}
