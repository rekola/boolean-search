# boolean-search

[![GitHub License](https://img.shields.io/github/license/rekola/boolean-search?logo=github&logoColor=lightgrey&color=yellow)](https://github.com/rekola/boolean-search/blob/main/LICENSE)
[![CI](https://github.com/rekola/boolean-search/workflows/Ubuntu-CI/badge.svg)]()
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg?style=flat-square)](http://makeapullrequest.com)

A single-header C++ Boolean search library for streaming applications.

## Features

- UTF-8
- AND, OR, NOT, NEAR, ONEAR operators
- Wildcards
- Unicode normalization

## Example

```c++
boolean_matcher::matcher m("apple AND orange");
if (m.match("I've got an apple and an orange")) {
	std::cout << "A match was found\n";
}
```

## Future Plans

- Add maximum distance to NEAR and ONEAR (e.g. `NEAR/1`)
- Add support for wstrings
- Add support for pairs and tuples
- Add interface for metadata queries (e.g. `.timestamp > "2024-10-01"`)
- Allow wildcards inside terms (e.g. `w*d`)
- Add comparison operators and arithmetics
- Add filtering
- Add better lexer and parser
- Embeddings / Document vectors

## Dependencies

- utf8proc
