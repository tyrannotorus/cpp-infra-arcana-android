// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef XML_HPP
#define XML_HPP

#include "tinyxml2.h"
#include <string>

namespace xml
{
using Doc = tinyxml2::XMLDocument;
using Element = tinyxml2::XMLElement;

void load_file(const std::string& path, Doc& to_doc);

Element* first_child(Doc& doc);

Element* first_child(Element* e, const std::string& name = "");

bool has_child(Element* e, const std::string& name);

Element* next_sibling(Element* e, const std::string& name = "");

std::string get_text_str(const Element* e);

bool get_text_bool(const Element* e);

int get_text_int(const Element* e);

std::string get_attribute_str(const Element* e, const std::string& name);

int get_attribute_int(const Element* e, const std::string& name);

bool try_get_attribute_str(
        const Element* e,
        const std::string& name,
        std::string& result);

bool try_get_attribute_int(
        const Element* e,
        const std::string& name,
        int& result);

bool try_get_attribute_bool(
        const Element* e,
        const std::string& name,
        bool& result);

}  // namespace xml

#endif  // XML_HPP
