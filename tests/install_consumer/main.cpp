#include <iostream>
#include <wikilib.hpp>
#if __cplusplus < 202100L
#error "wikilib must export its C++23 requirement"
#endif

int main() {
    using namespace wikilib;
    auto provider = std::make_shared<templates::MemoryTemplateProvider>();
    provider->add_template("Greeting", "Hello {{{1|world}}}, {{PAGENAME}}!");
    templates::TemplateExpander expander(provider);
    PageInfo page;
    page.title = "Consumer";
    if (page.full_title() != "Consumer")
        return 1;
    const auto expanded = expander.expand("{{Greeting|reader}} {{#expr:2+3*4}}", page);
    if (!expanded || *expanded != "Hello reader, Consumer! 14")
        return 2;
    auto parsed = markup::parse("{{Greeting|reader}}");
    if (!parsed.document || !expander.expand_ast(*parsed.document, page))
        return 3;
    if (parsed.document->to_plain_text().find("Hello reader") == std::string::npos)
        return 4;
    const auto compressed = dump::compress_bz2("consumer");
    if (!compressed)
        return 5;
    const auto decompressed = dump::decompress_bz2(*compressed);
    if (!decompressed || *decompressed != "consumer")
        return 6;
    if (text::trim(" padded ") != "padded")
        return 7;
    if (unicode::to_upper("ż") != "Ż")
        return 8;
    if (output::to_json(*parsed.document).empty())
        return 9;
    std::cout << "installed consumer: templates, AST, compression, C++23 OK\n";
}
