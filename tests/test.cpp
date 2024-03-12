#include <catch2/catch_test_macros.hpp>

#include "boolean_search.h"

#include <iostream>

TEST_CASE( "expression with term only", "[term]" ) {
  boolean_matcher::matcher m("hello");

  REQUIRE(m.match("Hello world!") == true);
  REQUIRE(m.match("Goodbye world!") == false);
  REQUIRE(m.match("Hello!") == true);
  REQUIRE(m.match("Helloo!") == false);
}

TEST_CASE( "AND operation", "[and]" ) {
  boolean_matcher::matcher m("apple AND orange");

  REQUIRE(m.match("I've got an apple and an orange") == true);
  REQUIRE(m.match("I've only got an apple") == false);
  REQUIRE(m.match("I've only got an orange") == false);
}

TEST_CASE( "OR operation", "[or]") {
  boolean_matcher::matcher m("war OR peace");

  REQUIRE(m.match("There is a war going on") == true);
  REQUIRE(m.match("I want peace") == true);
  REQUIRE(m.match("Hello world!") == false);
}

TEST_CASE( "NOT operation", "[not]") {
  boolean_matcher::matcher m("one NOT (two OR three)");

  REQUIRE(m.match("one two") == false);
  REQUIRE(m.match("one three") == false);
  REQUIRE(m.match("ZERO ONE") == true);
  REQUIRE(m.match("apple orange") == false);
  REQUIRE(m.match("two three") == false);
}

TEST_CASE( "NEAR operation", "[near]") {
  boolean_matcher::matcher m("happy NEAR human");

  REQUIRE(m.match("There is a sad human in the room") == false);
  REQUIRE(m.match("There is a happy human drinking coffee") == true);
  REQUIRE(m.match("The cat is happy, that's evident, but the human is not") == false);
  REQUIRE(m.match("Are you happy?") == false);
  REQUIRE(m.match("No humans here.") == false);
}

TEST_CASE( "ONEAR operation", "[onear]") {
  boolean_matcher::matcher m("beautiful ONEAR Martian");

  REQUIRE(m.match("There is a beautiful Martian at the door.") == true);
  REQUIRE(m.match("The Martian is not actually beautiful") == false);  
}

TEST_CASE( "search", "[search]" ) {
  boolean_matcher::matcher m("irure AND reprehenderit");
  auto s = std::string{ "Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
			"sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
			"Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris "
			"nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in "
			"reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
			"pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
			"culpa qui officia deserunt mollit anim id est laborum." };

  auto r = m.search(s);
  REQUIRE(r.has_match());
}

TEST_CASE( "wildcards", "[wildcards]" ) {
  boolean_matcher::matcher m("hello*");
  REQUIRE(m.match("Hell") == false);
  REQUIRE(m.match("Hello!") == true);
  REQUIRE(m.match("Helloo!") == true);
  REQUIRE(m.match("Hellooou!") == true);

  boolean_matcher::matcher m2("*ello");
  REQUIRE(m2.match("ello") == true);
  REQUIRE(m2.match("hello") == true);
  REQUIRE(m2.match("cello") == true);
  REQUIRE(m2.match("llo") == false);

  boolean_matcher::matcher m3("*ello*");
  REQUIRE(m3.match("ello") == true);
  REQUIRE(m3.match("yellow") == true);
  REQUIRE(m3.match("helo") == false);

  boolean_matcher::matcher m4("*");
  REQUIRE(m4.match("hello world!") == true);

  boolean_matcher::matcher m5("* AND world");
  REQUIRE(m5.match("hello world!") == true);
}

TEST_CASE( "n-grams", "[n-grams]" ) {
  boolean_matcher::matcher m("\"hello world\"");
  REQUIRE(m.match("Let me start by saying: Hello world!") == true);
  REQUIRE(m.match("World hello") == false);
  REQUIRE(m.match("hello worldddd") == false);
  REQUIRE(m.match("Hello beautiful world") == false);

  boolean_matcher::matcher m2("\"one two three\" AND four");
  REQUIRE(m2.match("one two three four") == true);
  REQUIRE(m2.match("four one two three") == true);
  REQUIRE(m2.match("one two three") == false);
  REQUIRE(m2.match("four one") == false);

  boolean_matcher::matcher m3("\"hello world*\"");
  REQUIRE(m3.match("hello world") == true);
  REQUIRE(m3.match("hello worlddddd") == true);
  REQUIRE(m3.match("hello beautiful world") == false);
}

TEST_CASE( "implicit ORs", "[implicit_or]" ) {
  boolean_matcher::matcher m("hello world");
  REQUIRE(m.match("hello") == true);
  REQUIRE(m.match("world") == true);
  REQUIRE(m.match("world hello") == true);
  REQUIRE(m.match("orange") == false);
}
