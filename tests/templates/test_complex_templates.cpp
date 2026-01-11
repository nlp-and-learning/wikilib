/**
 * @file test_complex_templates.cpp
 * @brief Complex real-world template invocation tests
 */

#include <gtest/gtest.h>
#include "wikilib/templates/template_parser.h"

using namespace wikilib::templates;

// ============================================================================
// Complex real-world template - Infobox language
// ============================================================================

// Full Infobox language template from Wikipedia Polish language article
static constexpr std::string_view INFOBOX_LANGUAGE_POLISH = R"({{Infobox language
| name             = Polish
| nativename       = {{lang|pl|polski}}
| pronunciation    = {{IPA|pl|ˈpɔlskʲi||Pl-polski.ogg}}
| states           = [[Poland]], [[Lithuania]], and bordering regions
| speakers         = [[first language|L1]]: {{sigfig|39.709620|2}} million
| date             = 2021
| ref              = e27
| speakers2        = [[Second language|L2]]: {{sigfig|2.092000|2}} million (2021)<ref name=e27/><br>Total: {{sigfig|42.629030|2}} million (2021)<ref name=e27/>
| speakers_label   = Speakers
| familycolor      = Indo-European
| fam2             = [[Balto-Slavic languages|Balto-Slavic]]
| fam3             = [[Slavic languages|Slavic]]
| fam4             = [[West Slavic languages|West Slavic]]
| fam5             = [[Lechitic languages|Lechitic]]
| fam6             = [[East Lechitic dialects|East Lechitic]]
| dia1             = [[Greater Poland dialect group|Greater Poland]]
| dia2             = [[Goral ethnolect|Goral]]
| dia3             = [[Lesser Poland dialect group|Lesser Poland]]
| dia4             = [[Masovian dialect group|Masovian]]
| dia5             = [[New mixed dialects]]
| dia6             = [[Northern Borderlands dialect|Northern Borderlands]]
| dia7             = [[Silesian language|Silesian]]
| dia8             = [[Southern Borderlands dialect|Southern Borderlands]]
| ancestor         = [[Old Polish]]
| ancestor2        = [[Middle Polish]]
| script           = {{ubl|[[Latin script]] ([[Polish alphabet]])}}
| nation           = {{ubl|[[Poland]]|[[European Union]]}}
| minority         = {{ubl|[[Bosnia and Herzegovina]]|[[Brazil]]|[[Czech Republic]]}}
| agency           = [[Polish Language Council]]
| iso1             = pl
| iso2             = pol
| iso3             = pol
| lingua           = 53-AAA-cc [[West Slavic languages|53-AAA-b..-d]]
| map              = Map of Polish language.svg
| mapcaption       = {{legend|#0080FE|Majority of Polish speakers}}
| notice           = IPA
| sign             = [[Sign Language System]]
| glotto           = poli1260
| glottorefname    = Polish
}})";

TEST(ComplexTemplateTest, InfoboxLanguage_ParseSuccess) {
    auto result = parse_invocation(INFOBOX_LANGUAGE_POLISH);

    ASSERT_TRUE(result.has_value()) << "Failed to parse Infobox language template";
    EXPECT_EQ(result->name, "Infobox language");
}

TEST(ComplexTemplateTest, InfoboxLanguage_ParameterCount) {
    auto result = parse_invocation(INFOBOX_LANGUAGE_POLISH);

    ASSERT_TRUE(result.has_value());

    // Count parameters (excluding template name)
    EXPECT_GT(result->parameters.size(), 30u) << "Expected at least 30 parameters";
    EXPECT_LT(result->parameters.size(), 60u) << "Expected less than 60 parameters";
}

TEST(ComplexTemplateTest, InfoboxLanguage_GetSimpleValue) {
    auto result = parse_invocation(INFOBOX_LANGUAGE_POLISH);

    ASSERT_TRUE(result.has_value());

    // Test simple string values
    auto name = result->get("name");
    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(*name, "Polish");

    auto date = result->get("date");
    ASSERT_TRUE(date.has_value());
    EXPECT_EQ(*date, "2021");

    auto ref = result->get("ref");
    ASSERT_TRUE(ref.has_value());
    EXPECT_EQ(*ref, "e27");
}

TEST(ComplexTemplateTest, InfoboxLanguage_GetISOCodes) {
    auto result = parse_invocation(INFOBOX_LANGUAGE_POLISH);

    ASSERT_TRUE(result.has_value());

    auto iso1 = result->get("iso1");
    ASSERT_TRUE(iso1.has_value());
    EXPECT_EQ(*iso1, "pl");

    auto iso2 = result->get("iso2");
    ASSERT_TRUE(iso2.has_value());
    EXPECT_EQ(*iso2, "pol");

    auto iso3 = result->get("iso3");
    ASSERT_TRUE(iso3.has_value());
    EXPECT_EQ(*iso3, "pol");
}

TEST(ComplexTemplateTest, InfoboxLanguage_GetGlottocode) {
    auto result = parse_invocation(INFOBOX_LANGUAGE_POLISH);

    ASSERT_TRUE(result.has_value());

    auto glotto = result->get("glotto");
    ASSERT_TRUE(glotto.has_value());
    EXPECT_EQ(*glotto, "poli1260");

    auto glottorefname = result->get("glottorefname");
    ASSERT_TRUE(glottorefname.has_value());
    EXPECT_EQ(*glottorefname, "Polish");
}

TEST(ComplexTemplateTest, InfoboxLanguage_GetNonexistentKey) {
    auto result = parse_invocation(INFOBOX_LANGUAGE_POLISH);

    ASSERT_TRUE(result.has_value());

    // dia9 doesn't exist (we only have dia1-dia8)
    auto dia9 = result->get("dia9");
    EXPECT_FALSE(dia9.has_value());

    auto nonexistent = result->get("nonexistent_key");
    EXPECT_FALSE(nonexistent.has_value());
}

TEST(ComplexTemplateTest, InfoboxLanguage_GetWithDefault) {
    auto result = parse_invocation(INFOBOX_LANGUAGE_POLISH);

    ASSERT_TRUE(result.has_value());

    // Existing key - should return actual value
    EXPECT_EQ(result->get_or("iso1", "unknown"), "pl");

    // Non-existing key - should return default
    EXPECT_EQ(result->get_or("dia9", "N/A"), "N/A");
    EXPECT_EQ(result->get_or("missing", "default_value"), "default_value");
}

TEST(ComplexTemplateTest, InfoboxLanguage_GetDialects) {
    auto result = parse_invocation(INFOBOX_LANGUAGE_POLISH);

    ASSERT_TRUE(result.has_value());

    // Check all 8 dialects
    auto dia1 = result->get("dia1");
    ASSERT_TRUE(dia1.has_value());
    EXPECT_TRUE(dia1->find("Greater Poland") != std::string_view::npos);

    auto dia2 = result->get("dia2");
    ASSERT_TRUE(dia2.has_value());
    EXPECT_TRUE(dia2->find("Goral") != std::string_view::npos);

    auto dia8 = result->get("dia8");
    ASSERT_TRUE(dia8.has_value());
    EXPECT_TRUE(dia8->find("Southern Borderlands") != std::string_view::npos);
}

TEST(ComplexTemplateTest, InfoboxLanguage_GetValueWithWikilinks) {
    auto result = parse_invocation(INFOBOX_LANGUAGE_POLISH);

    ASSERT_TRUE(result.has_value());

    // Value contains wikilinks: [[Sign Language System]]
    auto sign = result->get("sign");
    ASSERT_TRUE(sign.has_value());
    EXPECT_EQ(*sign, "[[Sign Language System]]");

    // Value contains wikilinks: [[Poland]], [[Lithuania]], and bordering regions
    auto states = result->get("states");
    ASSERT_TRUE(states.has_value());
    EXPECT_TRUE(states->find("[[Poland]]") != std::string_view::npos);
    EXPECT_TRUE(states->find("[[Lithuania]]") != std::string_view::npos);
}

TEST(ComplexTemplateTest, InfoboxLanguage_GetNestedTemplate_AsString) {
    auto result = parse_invocation(INFOBOX_LANGUAGE_POLISH);

    ASSERT_TRUE(result.has_value());

    // nativename contains nested template: {{lang|pl|polski}}
    auto nativename = result->get("nativename");
    ASSERT_TRUE(nativename.has_value());
    EXPECT_EQ(*nativename, "{{lang|pl|polski}}");

    // pronunciation contains nested template: {{IPA|pl|ˈpɔlskʲi||Pl-polski.ogg}}
    auto pronunciation = result->get("pronunciation");
    ASSERT_TRUE(pronunciation.has_value());
    EXPECT_TRUE(pronunciation->find("{{IPA|") != std::string_view::npos);

    // script contains nested template: {{ubl|[[Latin script]] ([[Polish alphabet]])}}
    auto script = result->get("script");
    ASSERT_TRUE(script.has_value());
    EXPECT_TRUE(script->find("{{ubl|") != std::string_view::npos);
}

TEST(ComplexTemplateTest, InfoboxLanguage_ParseNestedTemplate) {
    auto result = parse_invocation(INFOBOX_LANGUAGE_POLISH);

    ASSERT_TRUE(result.has_value());

    // Get nested template as string
    auto nativename = result->get("nativename");
    ASSERT_TRUE(nativename.has_value());

    // Parse the nested template
    auto nested = parse_invocation(*nativename);
    ASSERT_TRUE(nested.has_value()) << "Failed to parse nested {{lang}} template";

    EXPECT_EQ(nested->name, "lang");
    ASSERT_GE(nested->parameters.size(), 2u);

    // Get positional parameters from nested template
    auto lang_code = nested->get(1);
    ASSERT_TRUE(lang_code.has_value());
    EXPECT_EQ(*lang_code, "pl");

    auto text = nested->get(2);
    ASSERT_TRUE(text.has_value());
    EXPECT_EQ(*text, "polski");
}

TEST(ComplexTemplateTest, InfoboxLanguage_ParseMultipleNestedTemplates) {
    auto result = parse_invocation(INFOBOX_LANGUAGE_POLISH);

    ASSERT_TRUE(result.has_value());

    // Get mapcaption which contains {{legend|...}}
    auto mapcaption = result->get("mapcaption");
    ASSERT_TRUE(mapcaption.has_value());

    // Parse the nested legend template
    auto legend = parse_invocation(*mapcaption);
    ASSERT_TRUE(legend.has_value()) << "Failed to parse nested {{legend}} template";

    EXPECT_EQ(legend->name, "legend");
    ASSERT_GE(legend->parameters.size(), 2u);

    // First parameter is color
    auto color = legend->get(1);
    ASSERT_TRUE(color.has_value());
    EXPECT_EQ(*color, "#0080FE");

    // Second parameter is description
    auto description = legend->get(2);
    ASSERT_TRUE(description.has_value());
    EXPECT_EQ(*description, "Majority of Polish speakers");
}

TEST(ComplexTemplateTest, InfoboxLanguage_IterateAllParameters) {
    auto result = parse_invocation(INFOBOX_LANGUAGE_POLISH);

    ASSERT_TRUE(result.has_value());

    // Count named vs positional parameters
    int named_count = 0;
    int positional_count = 0;

    for (const auto& [name, value] : result->parameters) {
        if (name.empty()) {
            positional_count++;
        } else {
            named_count++;
        }
    }

    // Infobox uses mostly named parameters
    EXPECT_GT(named_count, 30);
    // Some whitespace lines may be parsed as positional parameters
    EXPECT_LT(positional_count, 20);
}

TEST(ComplexTemplateTest, InfoboxLanguage_CheckParameterNames) {
    auto result = parse_invocation(INFOBOX_LANGUAGE_POLISH);

    ASSERT_TRUE(result.has_value());

    // Check that specific parameters exist
    std::vector<std::string> expected_params = {
        "name", "nativename", "pronunciation", "states", "speakers",
        "date", "ref", "familycolor", "fam2", "fam3", "fam4", "fam5", "fam6",
        "dia1", "dia2", "dia3", "dia4", "dia5", "dia6", "dia7", "dia8",
        "ancestor", "ancestor2", "script", "nation", "minority", "agency",
        "iso1", "iso2", "iso3", "lingua", "map", "mapcaption",
        "notice", "sign", "glotto", "glottorefname"
    };

    for (const auto& param : expected_params) {
        auto value = result->get(param);
        EXPECT_TRUE(value.has_value()) << "Missing parameter: " << param;
    }
}

// ============================================================================
// Nested template extraction tests
// ============================================================================

TEST(ComplexTemplateTest, ExtractNestedTemplatesFromValue) {
    auto result = parse_invocation(INFOBOX_LANGUAGE_POLISH);

    ASSERT_TRUE(result.has_value());

    // nativename contains nested template: {{lang|pl|polski}}
    auto nativename = result->get("nativename");
    ASSERT_TRUE(nativename.has_value());

    // Find nested template
    auto nested_templates = find_invocations(*nativename);

    // Should find {{lang|...}} template
    ASSERT_GE(nested_templates.size(), 1u);
    EXPECT_EQ(nested_templates[0].name, "lang");
}

TEST(ComplexTemplateTest, MultiLevelNesting) {
    // Create a template with multiple levels of nesting
    std::string multi_level = "{{Outer|param={{Middle|value={{Inner|data}}}}}}";

    auto outer = parse_invocation(multi_level);
    ASSERT_TRUE(outer.has_value());
    EXPECT_EQ(outer->name, "Outer");

    // Get nested template value
    auto param_value = outer->get("param");
    ASSERT_TRUE(param_value.has_value());
    EXPECT_EQ(*param_value, "{{Middle|value={{Inner|data}}}}");

    // Parse middle level
    auto middle = parse_invocation(*param_value);
    ASSERT_TRUE(middle.has_value());
    EXPECT_EQ(middle->name, "Middle");

    // Get inner template value
    auto value_param = middle->get("value");
    ASSERT_TRUE(value_param.has_value());
    EXPECT_EQ(*value_param, "{{Inner|data}}");

    // Parse innermost level
    auto inner = parse_invocation(*value_param);
    ASSERT_TRUE(inner.has_value());
    EXPECT_EQ(inner->name, "Inner");

    auto data = inner->get(1);
    ASSERT_TRUE(data.has_value());
    EXPECT_EQ(*data, "data");
}
