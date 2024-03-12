#ifndef _BOOLEAN_MATCHER_H_
#define _BOOLEAN_MATCHER_H_

#include <codecvt>
#include <locale>
#include <string_view>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <deque>
#include <stdexcept>

#include <utf8proc.h>

namespace boolean_matcher {
  constexpr char32_t BOUNDARY = 0xe000;

  // data for a single matching term
  struct match_data {
    match_data() : pos_(0), size_(0), word_index_(0) { }
    match_data(int pos, int size, int word_index) : pos_(pos), size_(size), word_index_(word_index) { }
    
    int pos_, size_, word_index_;
  };

  // the main class
  class matcher {
  public:

    // result of a search
    class result {
    public:
      explicit result(std::u32string input) noexcept : input_(std::move(input)) { }
      explicit result(std::u32string input, std::vector<match_data> matches) noexcept : input_(std::move(input)), matches_(std::move(matches)) { }
      
      bool has_match() const noexcept { return !matches_.empty(); }
      
      std::string get_hit_sentence() const noexcept {
	if (!matches_.empty()) {
	  auto & match0 = matches_.front();
	  auto i0 = match0.pos_;
	  auto i1 = i0 + match0.size_;
	  for (int k = 0; k < 2; k++) {
	    if (i0 > 0) {
	      auto pos = input_.rfind(' ', i0 - 1);
	      if (pos != std::string::npos) i0 = pos;
	      else i0 = 0;
	    } 
	    if (i1 < static_cast<int>(input_.size())) {
	      auto pos = input_.find(' ', i1 + 1);
	      if (pos != std::string::npos) i1 = pos;
	      else i1 = input_.size();
	    }
	  }
	  auto s = input_.substr(i0, i1 - i0);
	  std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
	  auto hit_sentence = converter.to_bytes(s.data(), s.data() + s.size());
	  
	  if (i0 > 0) hit_sentence = "… " + hit_sentence;
	  if (i1 < static_cast<int>(input_.size())) hit_sentence += " …";
	  
	  return hit_sentence;
	} else {
	  return "";
	}
      }
      
    private:
      std::u32string input_;
      std::vector<match_data> matches_;
    };
    
    explicit matcher(std::string_view expression)
      : expression_(parse(expression)) { }

    // returns true if the matcher matches text
    bool match(std::string_view text) {
      initialize();
      auto text1 = normalize(text);
      auto text2 = converter_.from_bytes(text1.data(), text1.data() + text1.size());
      updateState(text2);
      return expression_->eval();
    }

    // returns extended search results for a text
    result search(std::string_view text) {
      initialize();
      auto text1 = normalize(text);
      auto text2 = converter_.from_bytes(text1.data(), text1.data() + text1.size());
      updateState(text2);
      return result(text2, expression_->getMatches());
    }

  private:
    // returns true if codepoint is a word character
    static bool isWordCharacter(char32_t codepoint) noexcept {
      auto cat = utf8proc_category(static_cast<utf8proc_int32_t>(codepoint));
      return cat == UTF8PROC_CATEGORY_LU || cat == UTF8PROC_CATEGORY_LL || cat == UTF8PROC_CATEGORY_LT || cat == UTF8PROC_CATEGORY_LM || cat == UTF8PROC_CATEGORY_LO || cat == UTF8PROC_CATEGORY_ND || cat == UTF8PROC_CATEGORY_PC;
    }

    // A node for the binary expression tree
    class Node {
    public:
      Node() { }
      Node(std::unique_ptr<Node> left, std::unique_ptr<Node> right)
	: left_(std::move(left)), right_(std::move(right)) { }
      
      virtual ~Node() { }

      virtual bool eval() const = 0;
      virtual std::vector<match_data> getMatches() const = 0;
      virtual void serialize(std::string & r) const = 0;
      virtual void addMatch(int pos, int word_index) { }

      virtual void reset() {
	if (left_) left_->reset();
	if (right_) right_->reset();
      }

      virtual void getTerms(std::vector<std::pair<std::u32string, Node *>> & r) {
	if (left_) left_->getTerms(r);
	if (right_) right_->getTerms(r);
      }
      
    protected:
      std::unique_ptr<Node> left_, right_;
    };

    // Term node contains the literal text to be found
    class Term : public Node {
    public:
      Term(std::string term0, std::u32string_view term) : term0_(std::move(term0)) {
	std::u32string suffix;
	if (!term.empty() && term.front() == '*') term.remove_prefix(1);
	else term_ += BOUNDARY;
	if (!term.empty() && term.back() == '*') term.remove_suffix(1);
	else suffix += BOUNDARY;
	bool prev_is_word = false;
	for (size_t i = 0; i < term.size(); i++) {
	  auto is_word = isWordCharacter(term[i]);
	  if (i > 0 && ((prev_is_word && !is_word) || (!prev_is_word && is_word))) {
	    term_ += BOUNDARY;
	  }
	  prev_is_word = is_word;
	  term_ += term[i];
	}
	term_ += suffix;
	for (size_t i = 0; i < term_.size(); i++) {
	  if (term_[i] > 0x20 && term_[i] != BOUNDARY) size_++;
	}
      }
      
      bool eval() const override {
	return !matches_.empty();
      }
      std::vector<match_data> getMatches() const override {
	return matches_;
      }
      void addMatch(int pos, int word_index) override {
	matches_.emplace_back(pos - size_ + 1, size_, word_index);
      }
      void reset() override {
	matches_.clear();
      }
      void getTerms(std::vector<std::pair<std::u32string, Node *>> & r) override {
	r.emplace_back(term_, this);
      }
      void serialize(std::string & r) const override {
	if (!r.empty()) r += " ";
	r += term0_;
      }

    private:
      std::string term0_;
      std::u32string term_;
      std::vector<match_data> matches_;
      int size_ = 0;
    };

    class And : public Node {
    public:
      And(std::unique_ptr<Node> left, std::unique_ptr<Node> right)
	: Node(std::move(left), std::move(right)) { }
    
      bool eval() const override {
	return left_->eval() && right_->eval();
      }
  
      std::vector<match_data> getMatches() const override {
	auto left_match = left_->getMatches();
	if (!left_match.empty()) {
	  auto right_match = right_->getMatches();
	  if (!right_match.empty()) {
	    std::copy(right_match.begin(), right_match.end(), std::back_inserter(left_match));
	    return left_match;
	  }
	}
	return std::vector<match_data>();
      }

      void serialize(std::string & r) const override {
	if (!r.empty()) r += " ";
	r += "(";
	left_->serialize(r);
	r += " AND";
	right_->serialize(r);
	r += ")";
      }
    };

    class Or : public Node {
    public:
      Or(std::unique_ptr<Node> left, std::unique_ptr<Node> right)
	: Node(std::move(left), std::move(right)) { }
      
      bool eval() const override {
	return left_->eval() || right_->eval();
      }

      std::vector<match_data> getMatches() const override {
	auto left_match = left_->getMatches();
	auto right_match = right_->getMatches();

	std::copy(right_match.begin(), right_match.end(), std::back_inserter(left_match));
    
	return left_match;
      }
    
      void serialize(std::string & r) const override {
	if (!r.empty()) r += " ";
	r += "(";
	left_->serialize(r);
	r += " OR";
	right_->serialize(r);
	r += ")";
      }
    };

    class AndNot : public Node {
    public:
      AndNot( std::unique_ptr<Node> left, std::unique_ptr<Node> right )
	: Node(std::move(left), std::move(right)) { }
    
      bool eval() const override {
	if (right_->eval()) return false;
	return left_->eval();
      }
  
      std::vector<match_data> getMatches() const override {
	if (right_->eval()) {
	  return std::vector<match_data>();
	} else {
	  return left_->getMatches();
	}
      }
  
      void serialize(std::string & r) const override {
	if (!r.empty()) r += " ";
	r += "(";
	left_->serialize(r);
	r += " NOT";
	right_->serialize(r);
	r += ")";
      }
    };

    class Near : public Node {
    public:
      Near(std::unique_ptr<Node> left, std::unique_ptr<Node> right, int left_distance, int right_distance)
	: Node(std::move(left), std::move(right)), left_distance_(left_distance), right_distance_(right_distance) { }

      bool eval() const override {
	auto matches = getMatches();
	return !matches.empty();
      }
      
      std::vector<match_data> getMatches() const override {
	std::vector<match_data> result;
	auto left_matches = left_->getMatches();
	
	if (!left_matches.empty()) {
	  auto right_matches = right_->getMatches();
	  
	  for (auto & left_match : left_matches) {
	    auto range_start = left_match.word_index_ - left_distance_;
	    auto range_end = left_match.word_index_ + right_distance_;
	    
	    for (auto & right_match : right_matches) {
	      if (right_match.word_index_ >= range_start && right_match.word_index_ <= range_end) {
		result.push_back(left_match);
		result.push_back(right_match);
	      }
	    }
	  }
	}
	
	return result;
      }
      
      void serialize(std::string & r) const override {
	if (!r.empty()) r += " ";
	r += "(";
	left_->serialize(r);
	if (left_distance_ == 0) r += " ONEAR";
	else r += " NEAR";
	right_->serialize(r);
	r += ")";
      }

    private:
      int left_distance_, right_distance_;
    };

    // A state for the Aho-Corasick String Search
    class SearchState {
    public:
      SearchState(char32_t character = 0) : character_(character) { }
      
      SearchState * findTransition(char32_t character) {
	auto it = transitions_.find(character);
	if (it == transitions_.end()) return nullptr;
	return it->second.get();
      }
      
      void addOutput(Node * node) {
	output_.insert(node);
      }
      
      std::unordered_set<Node *> & getOutput() { return output_; }
      
      std::unordered_map< char32_t, std::unique_ptr<SearchState> > & getTransitions() {
	return transitions_;
      }
      
      SearchState * getFailureTransition() { return failure_transition_; }
      void setFailureTransition(SearchState * state) { failure_transition_ = state; }
      
      char32_t getCharacter() const { return character_; }
      
      SearchState * getParent() { return parent_; }
      
      SearchState & addPattern(std::u32string_view pattern) {
	if (pattern.empty()) {
	  return *this;
	} else {
	  auto new_state = findTransition(pattern.front());
	  if (new_state) {
	    return new_state->addPattern(pattern.substr(1));
	  } else {
	    return createTransition(std::move(pattern));
	  }
	}
      }
      
      void computeFailureTransitions() {
	std::deque<SearchState *> q;
	
	for (auto & [ character, transition ] : transitions_) {
	  q.push_back(transition.get());
	  transition->setFailureTransition(this);
	}
	
	while (!q.empty()) {
	  auto state = q.front();
	  q.pop_front();
	  
	  auto failure_state = state->getParent()->getFailureTransition();
	  while (failure_state && failure_state->findTransition(state->getCharacter()) == nullptr) {
	    failure_state = failure_state->getFailureTransition();
	  }
	  
	  if (failure_state) {
	    state->setFailureTransition(failure_state->findTransition(state->getCharacter()));
	    for (auto & node : state->getFailureTransition()->getOutput()) {
	      state->addOutput(node);
	    }
	  } else {
	    state->setFailureTransition(this);
	  }
	  
	  for (auto & [ character, transition ] : state->getTransitions()) {
	    q.push_back(transition.get());
	  }
	}
	
	setFailureTransition(this);
      }
      
    private:
      SearchState * addTransition(std::unique_ptr<SearchState> to) {
	auto tmp = to.get();
	to->parent_ = this;
	transitions_[ to->character_ ] = std::move(to);
	return tmp;
      }
      
      SearchState & createTransition(std::u32string_view pattern) {
	if (pattern.empty()) {
	  return *this;
	} else {
	  auto child = addTransition(std::make_unique<SearchState>(pattern.front()));
	  return child->createTransition(pattern.substr(1));
	}
      }
      
      char32_t character_;
      SearchState * parent_ = nullptr;
      SearchState * failure_transition_ = nullptr;
      std::unordered_map< char32_t, std::unique_ptr<SearchState> > transitions_;
      std::unordered_set<Node *> output_;
    };
    
    void initialize() {
      if (current_state_) {
	// reset the state
	expression_->reset();
	current_pos_ = 0;
	current_word_ = 0;
      } else {
	// extract all terms from search query
	std::vector<std::pair<std::u32string, Node *>> terms;
	expression_->getTerms(terms);
	for (auto & [ term, node ] : terms) {
	  root_.addPattern(term).addOutput(node);
	}
	root_.computeFailureTransitions();
      }
      current_state_ = &root_;
    }

    // processes a string
    void updateState(std::u32string s) {
      bool prev_is_word = false;

      for (auto c : s) {
	auto is_word = isWordCharacter(c);
	bool is_word_start = false;
	if (!prev_is_word && is_word) {
	  is_word_start = true;
	  current_word_++;
	}
	if ((is_word_start || (prev_is_word && !is_word))) {
	  updateState(BOUNDARY);
	}
	prev_is_word = is_word;

	updateState(c);
      }
      
      if (prev_is_word) {
	updateState(BOUNDARY);
      }
    }
    
    // processes a single character
    void updateState(char32_t character) {
      auto pos = current_pos_;
      if (character != BOUNDARY) current_pos_++;

      auto new_state = current_state_->findTransition(character);

      while (new_state == nullptr) {
	if (current_state_ == &root_) break;
	current_state_ = current_state_->getFailureTransition();
	new_state = current_state_->findTransition(character);
      }
      
      if (new_state) {
	current_state_ = new_state;
      
	auto & output = current_state_->getOutput();
	
	for (auto node : output) {
	  node->addMatch(pos, current_word_);
	}
      }
    }

    // normalizes a string
    static std::string normalize(std::string_view input) noexcept {
      if (!input.empty()) {
	utf8proc_uint8_t * dest = nullptr;
	auto options = utf8proc_option_t(UTF8PROC_IGNORE | UTF8PROC_STRIPCC | UTF8PROC_CASEFOLD | UTF8PROC_COMPOSE);
	auto s = utf8proc_map(reinterpret_cast<const unsigned char *>(input.data()),
			      static_cast<utf8proc_ssize_t>(input.size()),
			      &dest,
			      options
			      );
	if (s >= 0) {
	  std::string r(reinterpret_cast<char *>(dest), static_cast<size_t>(s));
	  free(dest);
	  return r;
	} else {
	  free(dest);
	}
      }
      return std::string();
    }

    // tokenizes a string to words
    static std::deque<std::string> tokenize(std::string_view line) {
      std::deque<std::string> r;
      
      size_t pos0 = 0;
      while (pos0 < line.size()) {
	if (line[pos0] == ' ' || line[pos0] == '\t') {
	  pos0++;
	} else if (line[pos0] == '"') {
	  pos0++;
	  auto pos1 = line.find_first_of('"', pos0);
	  if (pos1 == std::string_view::npos) pos1 = line.size();
	  
	  r.emplace_back(line.substr(pos0, pos1 - pos0));
	  
	  pos0 = pos1 + 1;
	} else {
	  auto pos1 = line.find_first_of(" \t", pos0);
	  if (pos1 == std::string_view::npos) pos1 = line.size();
	  
	  if (pos0 < pos1) r.emplace_back(line.substr(pos0, pos1 - pos0));
	  
	  pos0 = pos1 + 1;
	}
      }
      
      return r;
    }

    // creates a binary expression tree from a expression string
    std::unique_ptr<Node> parse(std::string_view expression) {
      // add spaces before and after brackets to ease tokenization
      std::string e;
      for (size_t i = 0; i < expression.size(); i++) {
	auto c = expression[i];
	if (c == '(' || c == ')') {
	  if (!e.empty() && e.back() != ' ') e += ' ';
	  e += c;
	} else if (isspace(c)) {
	  if (!e.empty() && e.back() != ' ') e += ' ';
	} else {
	  if (!e.empty() && (e.back() == '(' || e.back() == ')')) e += ' ';
	  e += c;
	}
      }

      auto tokens = tokenize(e);

      std::vector<std::string> stack, rpn;
      while (!tokens.empty()) {
	auto t = std::move(tokens.front());
	tokens.pop_front();

	bool op1 = t == "AND" || t == "OR" || t == "NEAR" || t == "ONEAR" || t == "NOT";
	
	if (!tokens.empty()) {
	  auto & t2 = tokens.front();
	  bool op2 = t2 == "AND" || t2 == "OR" || t2 == "NEAR" || t2 == "ONEAR" || t2 == "NOT";
	  if (op1 && op2) throw std::runtime_error("missing term");
	  if (!op1 && t != "(" && !op2 && t2 != ")") {
	    tokens.push_front("OR");
	  }
	}
	
	if (op1 || t == "(") {
	  stack.push_back(t);
	} else if (t == ")") {
	  while (!stack.empty() && stack.back() != "(") {
	    rpn.push_back(stack.back());
	    stack.pop_back();
	  }
	  if (stack.empty()) throw std::runtime_error("mismatched parentheses");
	  stack.pop_back();
	} else {
	  rpn.push_back(t);
	}
      }

      while (!stack.empty()) {
	rpn.push_back(stack.back());
	stack.pop_back();
      }
  
      std::vector<std::unique_ptr<Node> > node_stack;
      
      for (auto & t : rpn) {
	if (t == "AND") {
	  if (node_stack.size() < 2) throw std::runtime_error("stack underflow");
	  
	  auto right = std::move(node_stack.back());
	  node_stack.pop_back();
	  auto left = std::move(node_stack.back());
	  node_stack.pop_back();
      
	  node_stack.push_back(std::make_unique<And>(std::move(left), std::move(right)));
	} else if (t == "OR") {
	  if (node_stack.size() < 2) throw std::runtime_error("stack underflow");

	  auto right = std::move(node_stack.back());
	  node_stack.pop_back();
	  auto left = std::move(node_stack.back());
	  node_stack.pop_back();
      
	  node_stack.push_back(std::make_unique<Or>(std::move(left), std::move(right)));
	} else if (t == "NEAR") {
	  if (node_stack.size() < 2) throw std::runtime_error("stack underflow");
      
	  auto right = std::move(node_stack.back());
	  node_stack.pop_back();
	  auto left = std::move(node_stack.back());
	  node_stack.pop_back();
      
	  node_stack.push_back(std::make_unique<Near>(std::move(left), std::move(right), 4, 4));
	} else if (t == "ONEAR") {
	  if (node_stack.size() < 2) throw std::runtime_error("stack underflow");
      
	  auto right = std::move(node_stack.back());
	  node_stack.pop_back();
	  auto left = std::move(node_stack.back());
	  node_stack.pop_back();
      
	  node_stack.push_back(std::make_unique<Near>(std::move(left), std::move(right), 0, 4));
	} else if (t == "NOT") {
	  if (node_stack.size() < 2) throw std::runtime_error("stack underflow");

	  auto right = std::move(node_stack.back());
	  node_stack.pop_back();
	  auto left = std::move(node_stack.back());
	  node_stack.pop_back();
      
	  node_stack.push_back(std::make_unique<AndNot>(std::move(left), std::move(right)));
	} else {
	  auto t2 = normalize(t);
	  auto t3 = converter_.from_bytes(t2.data(), t2.data() + t2.size());
	  node_stack.push_back(std::make_unique<Term>(t2, t3));
	}
      }

      if (node_stack.empty()) {
	throw std::runtime_error("no tokens");
      } else if (node_stack.size() > 1) {
	throw std::runtime_error("multiple node roots");
      } else {
	return std::move(node_stack.back());
      }
    }
    
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter_;

    int current_pos_ = 0, current_word_ = 0;
    std::unique_ptr<Node> expression_;
    
    SearchState * current_state_ = nullptr;
    SearchState root_;
  };
};

#endif
