# wikilib
C++ library for parsing MediaWiki markup (Wikipedia, Wiktionary, etc.)
                                                                                                                                                                
Components:                                                                                                                                                     
- markup: Tokenizer, parser, and AST for wikitext syntax                                                                                                        
- dump: XML reader with bzip2 decompression for Wikipedia dumps                                                                                                 
- output: JSON and plain text serialization                                                                                                                     
- templates: Basic template parsing and expansion                                                                                                               
                                                                                                                                                                  
Features:                                                                                                                                                       
- Full wikitext tokenization (links, templates, formatting, tables, lists)                                                                                      
- AST with visitor pattern for traversal and transformation                                                                                                     
- Streaming XML dump processing with page handlers                                                                                                              
- Index file support for random access to compressed dumps                                                                                                      
- JSON/JSONL output formats                                                                                                                                     
- Plain text extraction with configurable options    
