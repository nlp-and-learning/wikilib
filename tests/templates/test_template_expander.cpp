#include <gtest/gtest.h>
#include "wikilib/markup/parser.h"
#include "wikilib/templates/template_expander.h"

using namespace wikilib;
using namespace wikilib::templates;

namespace {
class ExpanderTest : public ::testing::Test {
protected:
    std::shared_ptr<MemoryTemplateProvider> provider = std::make_shared<MemoryTemplateProvider>();

    void SetUp() override {
        provider->add_template("Echo", "{{{1|fallback}}}/{{{name|default}}}");
        provider->add_template("Page", "{{PAGENAME}}:{{{1|none}}}");
        provider->add_template("Value", "value");
    }

    std::string expand(std::string_view text) {
        TemplateExpander expander(provider);
        auto result = expander.expand(text);
        EXPECT_TRUE(result) << (result ? "" : result.error().message);
        return result.value_or("ERROR");
    }
};

TEST_F(ExpanderTest, PositionalNamedEmptyAndLastDuplicateParameters) {
    EXPECT_EQ(expand("{{Echo|hello|name=world}}"), "hello/world");
    EXPECT_EQ(expand("{{Echo|1=first|1=last|name=}}"), "last/");
    EXPECT_EQ(expand("{{Echo}}"), "fallback/default");
    EXPECT_EQ(expand("{{Template:Value}}"), "value");
    EXPECT_EQ(expand("{{Echo|}}"), "/default");
}

TEST_F(ExpanderTest, BalancedNestedTemplatesParametersAndDefaults) {
    provider->add_template("Nested", "{{Echo|{{{1|{{Value}}}}}|name={{{missing|{{{other|D}}}}}}}}");
    EXPECT_EQ(expand("{{Nested}}"), "value/D");
    EXPECT_EQ(expand("{{Nested|{{Value}}}}"), "value/D");
    provider->add_template("Dynamic", "{{{ {{{key|1}}} |default}}}");
    EXPECT_EQ(expand("{{Dynamic|yes}}"), "yes");
}

TEST_F(ExpanderTest, LinkPipesAndEqualsDoNotSplitTemplateArguments) {
    EXPECT_EQ(expand("{{Echo|[[A|label=ok]]|name=world}}"), "[[A|label=ok]]/world");
    EXPECT_EQ(expand("{{Echo|name=a=b}}"), "fallback/a=b");
}

TEST_F(ExpanderTest, ExpandsDynamicTemplateNames) {
    provider->add_template("Name", "Value");
    EXPECT_EQ(expand("{{ {{Name}} }}"), "value");
}

TEST_F(ExpanderTest, CommentsAndNowikiAreOpaque) {
    const std::string text = "<!-- {{Value}} --><nowiki>{{Value}}</nowiki>{{Value}}";
    EXPECT_EQ(expand(text), "<!-- {{Value}} --><nowiki>{{Value}}</nowiki>value");
    EXPECT_EQ(expand("{{Echo|<nowiki>a|b=c</nowiki>}}"), "<nowiki>a|b=c</nowiki>/default");
    EXPECT_EQ(expand("<nowiki />{{Value}}"), "<nowiki />value");
}

TEST_F(ExpanderTest, PreservesUnclosedAndUndefinedParameters) {
    EXPECT_EQ(expand("pre {{unclosed"), "pre {{unclosed");
    EXPECT_EQ(expand("{{{missing}}}"), "{{{missing}}}");
    EXPECT_EQ(expand("{{{missing|}}}"), "");
}

TEST_F(ExpanderTest, MissingAndUnsupportedCallsPreserveAllArguments) {
    EXPECT_EQ(expand("{{Missing|a|name=b}}"), "{{Missing|a|name=b}}");
    EXPECT_EQ(expand("{{#unsupported:x|a=b}}"), "{{#unsupported:x|a=b}}");
    EXPECT_EQ(expand("{{#invoke:Module|function|name=value}}"), "{{#invoke:Module|function|name=value}}");
    ExpanderConfig config;
    config.evaluate_lua = true;
    TemplateExpander expander(provider, config);
    EXPECT_EQ(expander.expand("{{#invoke:Module|function}}").value(), "{{#invoke:Module|function}}");
}

TEST_F(ExpanderTest, StrictMissingErrorsPropagateFromNestedTemplates) {
    provider->add_template("Bad", "before {{Missing|arg}} after");
    ExpanderConfig config;
    config.fail_on_missing = true;
    TemplateExpander expander(provider, config);
    auto result = expander.expand("{{Bad}}");
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().message.find("Template not found: Missing"), std::string::npos);
    EXPECT_EQ(expander.stats().errors, 1);
}

TEST_F(ExpanderTest, CanDropUnknownAndDisableParserFunctions) {
    ExpanderConfig config;
    config.preserve_unknown = false;
    TemplateExpander expander(provider, config);
    EXPECT_EQ(expander.expand("x{{Missing|a}}y{{#invoke:M|f}}").value(), "xy");
    config.preserve_unknown = true;
    config.expand_parser_functions = false;
    TemplateExpander disabled(provider, config);
    EXPECT_EQ(disabled.expand("{{#expr:2+2}}").value(), "{{#expr:2+2}}");
}

TEST_F(ExpanderTest, IfIfeqAndIfexistChooseBranchesLazily) {
    ExpanderConfig config;
    config.fail_on_missing = true;
    TemplateExpander expander(provider, config);
    EXPECT_EQ(expander.expand("{{#if: yes|{{Value}}|{{Missing}}}}").value(), "value");
    EXPECT_EQ(expander.expand("{{#if: |{{Missing}}|no}}").value(), "no");
    EXPECT_EQ(expander.expand("{{#ifeq: {{Value}}|value|same|{{Missing}}}}").value(), "same");
    EXPECT_EQ(expander.expand("{{#ifexist:Value|yes|{{Missing}}}}").value(), "yes");
    EXPECT_EQ(expander.expand("{{#ifexist:Absent|{{Missing}}|no}}").value(), "no");
}

TEST_F(ExpanderTest, SwitchUsesCasesFallthroughAndLazyDefault) {
    EXPECT_EQ(expand("{{#switch:b|a=one|b=two|#default=other}}"), "two");
    EXPECT_EQ(expand("{{#switch:a|a|b=both|#default=other}}"), "both");
    EXPECT_EQ(expand("{{#switch:z|a=one|#default={{Value}}}}"), "value");
}

TEST_F(ExpanderTest, EvaluatesExpressionPrecedenceComparisonsAndBooleanOperators) {
    for (const auto &[input, expected]: std::vector<std::pair<std::string, std::string>>{{"2+3*4", "14"},
                                                                                         {"(2+3)*4", "20"},
                                                                                         {"2^3^2", "64"},
                                                                                         {"-2^2", "4"},
                                                                                         {"12/2*3", "18"},
                                                                                         {"7div2", "3"},
                                                                                         {"7mod3", "1"},
                                                                                         {"1e-3*1000", "1"},
                                                                                         {".5+.25", "0.75"},
                                                                                         {"2<=3 and not0", "1"},
                                                                                         {"0or1", "1"},
                                                                                         {"2<>2", "0"},
                                                                                         {"2!=3", "1"},
                                                                                         {"(2<3)+1", "2"},
                                                                                         {"1=1", "1"},
                                                                                         {"0-0", "0"}}) {
        EXPECT_EQ(expand("{{#expr:" + input + "}}"), expected) << input;
    }
}

TEST_F(ExpanderTest, IfexprUsesNumericResultAndEvaluatesOnlySelectedBranch) {
    ExpanderConfig config;
    config.fail_on_missing = true;
    TemplateExpander expander(provider, config);
    EXPECT_EQ(expander.expand("{{#ifexpr:2*3=6|yes|{{Missing}}}}").value(), "yes");
    EXPECT_EQ(expander.expand("{{#ifexpr:2-2|{{Missing}}|no}}").value(), "no");
    EXPECT_EQ(expander.expand("{{#ifexpr:-0.5|yes|no}}").value(), "yes");
}

TEST_F(ExpanderTest, MalformedAndNonFiniteExpressionsProduceErrors) {
    TemplateExpander expander(provider);
    for (const auto &expression:
         {"", "1/0", "1mod0", "2+", "(2", "2)", "1 2", "sqrt(4)", "1e999", "(-1)^.5", "notnot3", "1ornot0"}) {
        auto result = expander.expand("{{#expr:" + std::string(expression) + "}}");
        EXPECT_FALSE(result) << expression;
    }
    EXPECT_FALSE(expander.expand("{{#ifexpr:1/0|yes|no}}"));
    EXPECT_FALSE(expander.expand("{{#expr:" + std::string(130, '(') + "1" + std::string(130, ')') + "}}"));
}

TEST_F(ExpanderTest, CacheNeverReusesExpandedPageOrParameterContext) {
    TemplateExpander expander(provider);
    PageInfo page;
    page.title = "First";
    EXPECT_EQ(expander.expand("{{Page|a}} {{Page|b}}", page).value(), "First:a First:b");
    EXPECT_EQ(expander.stats().cache_hits, 1);
    page.title = "Second";
    EXPECT_EQ(expander.expand("{{Page|c}}", page).value(), "Second:c");
    provider->add_template("Page", "updated {{PAGENAME}}");
    EXPECT_EQ(expander.expand("{{Page}}", page).value(), "updated Second");
}

TEST_F(ExpanderTest, ProviderMayBeNullForExpressionsAndMagicWords) {
    TemplateExpander expander(nullptr);
    PageInfo page;
    page.title = "Root/Sub";
    EXPECT_EQ(expander.expand("{{BASEPAGENAME}} {{SUBPAGENAME}} {{#expr:2+2}}", page).value(), "Root Sub 4");
    EXPECT_FALSE(expander.expand("{{Value}}"));
}

TEST_F(ExpanderTest, ExpansionBudgetIsPerOperationAndIncludesCacheHits) {
    ExpanderConfig config;
    config.max_expansions = 1;
    TemplateExpander expander(provider, config);
    EXPECT_TRUE(expander.expand("{{Value}}"));
    EXPECT_TRUE(expander.expand("{{Value}}"));
    EXPECT_FALSE(expander.expand("{{Value}}{{Value}}"));
    EXPECT_FALSE(expander.expand("{{#expr:1}}{{Value}}"));
    EXPECT_TRUE(expander.expand("plain"));
    expander.reset_stats();
    EXPECT_TRUE(expander.expand("{{Value}}"));
    EXPECT_EQ(expander.stats().templates_expanded, 1);
}

TEST_F(ExpanderTest, RecursionLimitsAlwaysReturnErrorsAndRecover) {
    provider->add_template("Loop", "{{Loop}}");
    ExpanderConfig config;
    config.max_depth = 3;
    TemplateExpander expander(provider, config);
    EXPECT_FALSE(expander.expand("{{Loop}}"));
    EXPECT_TRUE(expander.expand("{{Value}}"));
    ExpansionContext context;
    context.max_depth = 0;
    TemplateInvocation invocation;
    invocation.name = "Value";
    EXPECT_FALSE(expander.expand_template(invocation, context));
    EXPECT_FALSE(expander.evaluate_parser_function(ParserFunction::Expr, {"1"}, context));
}

TEST_F(ExpanderTest, OutputLimitsApplyToPlainTextParametersAndDirectFunctions) {
    ExpanderConfig config;
    config.max_output_bytes = 4;
    TemplateExpander expander(provider, config);
    EXPECT_TRUE(expander.expand("1234"));
    EXPECT_FALSE(expander.expand("12345"));
    EXPECT_FALSE(expander.expand("{{Value}}"));
    EXPECT_FALSE(expander.evaluate_parser_function(ParserFunction::Tag, {"p", "text"}, {}));
}

TEST_F(ExpanderTest, ZeroLimitsAllowPlainTextButNoExpansionAndNegativeLimitsFail) {
    ExpanderConfig config;
    config.max_expansions = 0;
    TemplateExpander expander(provider, config);
    EXPECT_EQ(expander.expand("plain").value(), "plain");
    EXPECT_FALSE(expander.expand("{{Value}}"));
    config.max_depth = -1;
    TemplateExpander invalid(provider, config);
    EXPECT_FALSE(invalid.expand("plain"));
}

TEST_F(ExpanderTest, DirectApisExpandArgumentsAndShareErrorSemantics) {
    TemplateExpander expander(provider);
    auto invocation = parse_invocation("{{Echo|{{Value}}|name=yes}}");
    ASSERT_TRUE(invocation);
    EXPECT_EQ(expander.expand_template(*invocation, {}).value(), "value/yes");
    EXPECT_EQ(expander.evaluate_parser_function(ParserFunction::Expr, {"2+3"}, {}).value(), "5");
    EXPECT_EQ(expander.evaluate_parser_function(ParserFunction::Invoke, {"M", "f", "x=y"}, {}).value(),
              "{{#invoke:M|f|x=y}}");
}

TEST_F(ExpanderTest, AstExpansionCreatesMarkupAndRepairsParentsAndCategories) {
    provider->add_template("Markup", "== Section ==\n[[Target|label]]\n[[Category:Generated]]");
    auto parsed = markup::parse("{{Markup}}");
    ASSERT_TRUE(parsed.document);
    TemplateExpander expander(provider);
    auto result = expander.expand_ast(*parsed.document);
    ASSERT_TRUE(result) << (result ? "" : result.error().message);
    bool heading = false, link = false;
    std::function<void(const markup::Node &)> check = [&](const markup::Node &node) {
        heading |= node.type == markup::NodeType::Heading;
        link |= node.type == markup::NodeType::Link;
        for (const auto &child: node.children()) {
            EXPECT_EQ(child->parent, &node);
            check(*child);
        }
    };
    check(*parsed.document);
    EXPECT_TRUE(heading);
    EXPECT_TRUE(link);
    ASSERT_EQ(parsed.document->categories.size(), 1u);
    EXPECT_EQ(parsed.document->categories.front()->category, "Generated");
}

TEST_F(ExpanderTest, AstExpansionIsTransactionalOnError) {
    auto parsed = markup::parse("before {{Missing|a}} after");
    ASSERT_TRUE(parsed.document);
    const auto before = parsed.document->to_wikitext();
    ExpanderConfig config;
    config.fail_on_missing = true;
    TemplateExpander expander(provider, config);
    EXPECT_FALSE(expander.expand_ast(*parsed.document));
    EXPECT_EQ(parsed.document->to_wikitext(), before);
}

TEST_F(ExpanderTest, AstLeavesNowikiAndCommentsUnexpanded) {
    markup::ParserConfig config;
    config.tokenizer.preserve_comments = true;
    markup::Parser parser(config);
    auto parsed = parser.parse("<nowiki>{{Value}}</nowiki><!-- {{Value}} -->{{Value}}");
    ASSERT_TRUE(parsed.document);
    TemplateExpander expander(provider);
    ASSERT_TRUE(expander.expand_ast(*parsed.document));
    EXPECT_NE(parsed.document->to_wikitext().find("<nowiki>{{Value}}</nowiki>"), std::string::npos);
    EXPECT_NE(parsed.document->to_wikitext().find("<!-- {{Value}} -->"), std::string::npos);
    EXPECT_NE(parsed.document->to_wikitext().find("value"), std::string::npos);
}

TEST_F(ExpanderTest, LocalizedNamespacePageWordsAndUnsupportedMagicWords) {
    TemplateExpander expander(provider);
    PageInfo page;
    page.namespace_id = 10;
    page.title = "Szablon:Root/Sub";
    EXPECT_EQ(page.full_title(), "Szablon:Root/Sub");
    EXPECT_EQ(expander.expand("{{PAGENAME}}|{{FULLPAGENAME}}|{{NAMESPACE}}", page).value(),
              "Root/Sub|Szablon:Root/Sub|Szablon");
    EXPECT_EQ(expander.expand("{{NUMBEROFPAGES}} {{SERVER}} {{PAGENAMEE}}", page).value(),
              "{{NUMBEROFPAGES}} {{SERVER}} {{PAGENAMEE}}");
}

TEST_F(ExpanderTest, AstParserFunctionsAndPreservedTemplateChildrenRemainValid) {
    auto parsed = markup::parse("{{#expr:2+3*4}} {{Missing|{{Value}}}} {{PAGENAME}}");
    ASSERT_TRUE(parsed.document);
    PageInfo page;
    page.title = "Title";
    TemplateExpander expander(provider);
    auto result = expander.expand_ast(*parsed.document, page);
    ASSERT_TRUE(result) << (result ? "" : result.error().message);
    EXPECT_NE(parsed.document->to_wikitext().find("14"), std::string::npos);
    EXPECT_NE(parsed.document->to_wikitext().find("Title"), std::string::npos);
    std::function<void(const markup::Node &)> check = [&](const markup::Node &node) {
        for (const auto &child: node.children()) {
            EXPECT_EQ(child->parent, &node);
            check(*child);
        }
        if (node.type == markup::NodeType::Template)
            for (const auto &param: static_cast<const markup::TemplateNode &>(node).parameters)
                for (const auto &child: param.value) {
                    EXPECT_EQ(child->parent, &node);
                    check(*child);
                }
    };
    check(*parsed.document);
}

TEST_F(ExpanderTest, AstReparseErrorsDoNotChangeOriginalDocument) {
    std::string invalid;
    for (int i = 0; i < 120; ++i)
        invalid += "<span>";
    invalid += "text";
    for (int i = 0; i < 120; ++i)
        invalid += "</span>";
    provider->add_template("Invalid", invalid);
    auto parsed = markup::parse("{{Invalid}}");
    ASSERT_TRUE(parsed.document);
    const auto before = parsed.document->to_wikitext();
    TemplateExpander expander(provider);
    EXPECT_FALSE(expander.expand_ast(*parsed.document));
    EXPECT_EQ(parsed.document->to_wikitext(), before);
}

TEST_F(ExpanderTest, SimpleTagTimeAndUnsupportedFunctionVariants) {
    EXPECT_EQ(expand("{{#tag:span|{{Value}}}}"), "<span>value</span>");
    TemplateExpander expander(provider);
    const auto year = expander.expand("{{#time:Y}}");
    ASSERT_TRUE(year);
    EXPECT_EQ(year->size(), 4u);
    EXPECT_FALSE(expander.expand("{{#time:Y|2000-01-01}}"));
    EXPECT_FALSE(expander.expand("{{#tag:span|value|class=x}}"));
    EXPECT_EQ(expand("{{#language:pl}} {{#titleparts:A/B|1}}"), "{{#language:pl}} {{#titleparts:A/B|1}}");
}

TEST(ExpansionInvocationTest, NestedParameterLinkAndFunctionCaseSyntax) {
    auto inv = parse_invocation("{{T|{{{x|{{V}}}}}|[[A|label]]|name=a=b}}");
    ASSERT_TRUE(inv);
    ASSERT_EQ(inv->parameters.size(), 3u);
    EXPECT_EQ(inv->parameters[0].second, "{{{x|{{V}}}}}");
    EXPECT_EQ(inv->parameters[1].second, "[[A|label]]");
    EXPECT_EQ(inv->parameters[2].second, "a=b");
    EXPECT_FALSE(parse_invocation("{{T|unclosed"));
    EXPECT_FALSE(parse_invocation("{{{parameter}}}"));
    auto function = parse_invocation("{{#switch:x|x=yes|#default=no}}");
    ASSERT_TRUE(function);
    EXPECT_EQ(function->parameters[0].first, "");
    EXPECT_EQ(function->parameters[0].second, "x=yes");
    EXPECT_EQ(get_parser_function("#expr:2+2"), ParserFunction::Expr);
    EXPECT_TRUE(is_parser_function("#if:value"));
    const auto found = find_invocations("<!-- {{Hidden}} --><nowiki>{{Hidden}}</nowiki>"
                                        "{{{param|{{Hidden}}}}} {{T|{{{x|{{V}}}}}}}");
    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].name, "T");
    ASSERT_EQ(found[0].parameters.size(), 1u);
    EXPECT_EQ(found[0].parameters[0].second, "{{{x|{{V}}}}}");
}
} // namespace
