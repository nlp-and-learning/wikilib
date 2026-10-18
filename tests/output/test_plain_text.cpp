#include <gtest/gtest.h>
#include "wikilib/markup/tokenizer.h"
#include "wikilib/output/plain_text.h"

using namespace wikilib;
using namespace wikilib::output;

TEST(PlainTextTest, SkipsSelfClosingRefs) {
    EXPECT_EQ(to_plain_text(R"(immunologiczne<ref name="cmkp" /><ref name="PWN"/>. W Polsce)"),
              "immunologiczne. W Polsce");
}

TEST(PlainTextTest, SkipsRefWithNestedMarkup) {
    EXPECT_EQ(to_plain_text("A<ref>Zob. [[Ustawa]] o <small>lekarzach</small>, s. 5.</ref> B"), "A B");
}

TEST(PlainTextTest, SkipsRefWithAttributesAndNewlines) {
    EXPECT_EQ(to_plain_text("elektronu<ref group=uwaga>Kolor\npunktu.</ref>. Dalej"), "elektronu. Dalej");
}

TEST(PlainTextTest, SkipsRefsCaseInsensitive) {
    EXPECT_EQ(to_plain_text("A<REF>x</Ref> B<BR>C"), "A B\nC");
}

TEST(PlainTextTest, SkipsReferencesAndMath) {
    EXPECT_EQ(to_plain_text("Wzór <math>E = mc^2</math> tu.\n\n<references>\n<ref name=a>x</ref>\n</references>"),
              "Wzór tu.");
}

TEST(PlainTextTest, UnclosedRefKeepsFollowingText) {
    EXPECT_EQ(to_plain_text("A<ref name=x> B"), "A B");
}

TEST(PlainTextTest, StrayClosingRefIsDropped) {
    EXPECT_EQ(to_plain_text("A</ref> B"), "A B");
}

TEST(PlainTextTest, KeepsPoemText) {
    EXPECT_EQ(to_plain_text("<poem>Litwo, ojczyzno moja</poem>"), "Litwo, ojczyzno moja");
}

TEST(PlainTextTest, HeadingWithRefAndMarkup) {
    EXPECT_EQ(to_plain_text("== Nagrody<ref name=\"ppibl\" /> ==\nTekst"), "Nagrody \nTekst");
    EXPECT_EQ(to_plain_text("== [[Wiedźmin|Saga]] i '''inne''' ==\nTekst"), "Saga i inne \nTekst");
}

TEST(PlainTextTest, HeadingStartingWithListMarker) {
    EXPECT_EQ(to_plain_text("==*Gwiazdka==\nTekst"), "*Gwiazdka\nTekst");
}

TEST(PlainTextTest, DecodesEntities) {
    EXPECT_EQ(to_plain_text("10&nbsp;km &ndash; o&nbsp;&nbsp;tym"), "10 km – o tym");
    EXPECT_EQ(to_plain_text("A&#124;B &#x2236; &lt;font&gt; &amp;lt;"), "A|B ∶ <font> &lt;");
}

TEST(PlainTextTest, ExtensionTagNames) {
    EXPECT_TRUE(markup::is_extension_tag("ref"));
    EXPECT_TRUE(markup::is_extension_tag("REFERENCES"));
    EXPECT_TRUE(markup::is_extension_tag("math"));
    EXPECT_FALSE(markup::is_extension_tag("span"));
    EXPECT_FALSE(markup::is_extension_tag("refx"));
}
