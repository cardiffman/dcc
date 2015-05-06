/*
 * dccsuper.cpp
 *
 *  Created on: Apr 26, 2015
 *      Author: Mike
 */
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <algorithm>
#include <stdlib.h>
#include <string.h>

using namespace std;
static vector<int> indented;

enum TokenType {
	TT_VARID, TT_CONID, TT_INTEGER, TT_DOUBLE, TT_STRING, TT_OPER, TT_DATA,
	TT_LPAREN, TT_RPAREN,
	TT_SEMI, TT_INDENT, TT_OUTDENT,
	TT_EOF
};
const char* TokenTypeStr[] =
{
	"TT_VARID", "TT_CONID", "TT_INTEGER", "TT_DOUBLE", "TT_STRING", "TT_OPER",
	"TT_LPAREN", "TT_RPAREN", "TT_DATA",
	"TT_SEMI", "TT_INDENT", "TT_OUTDENT",
	"TT_EOF"
};
ostream& operator<<(ostream& out, TokenType type)
{
	return out << TokenTypeStr[type];
}
struct Token {
	Token() : type(TT_EOF) {}
	Token(TokenType type) : type(type){}
	TokenType type;
	string text;
	double dval = 0;
	int ival = 0;
	ostream& print(ostream& out) const;
};
ostream& Token::print(ostream& out) const
{
	out << type;
	switch (type)
	{
	case TT_VARID: case TT_CONID: case TT_STRING: case TT_OPER:
		out << ' ' << text;
		break;
	case TT_INTEGER:
		out << ' ' << ival;
		break;
	case TT_DOUBLE:
		out << ' ' << dval;
		break;
	//TT_INDENT, TT_OUTDENT,
	//TT_EOF
	}
	return out;
}
ostream& operator<<(ostream& out, const Token& token)
{
	return token.print(out);
}

string lastline;

bool ascSymbol(int ch)
{
	return string("!#$%&*+./<=>?@\\^|-~:").find(ch)!=string::npos;
}
int line_number = 0;
struct Error {
	int line;
	string msg;
	Error(int line, const string& msg) : line(line), msg(msg) {}
};
/*
 * An implementation of basically the Python indent system.
 * Not really needed for the language I'm parsing because there
 * are no blocks. With a case block or a let/where block this would be
 * handy.
 * This system may do things redundantly. The alternative would be
 * to use a token buffer to help insert the inferred tokens.
 * Contrast with Haskell:
 * Haskell wants an opening curly brace after certain keywords, such as
 * the 'where' at the beginning of a module. Either the user provides
 * or it is inferred. So in that system the rules are trickier. Python
 * only uses curly braces for dictionary literals and the indent and
 * dedent are completely separate tokens. So indentation is obligatory
 * in Python, while Haskell is okay with explicit curly braces.
 */
list<int> columns;
bool IndentDedentHandling(istream& in, Token& inferred)
{
	static int column = 0;
	static bool newline_done = false;
	if (columns.empty())
		columns.push_back(0);

	while (in.good())
	{
		while (lastline.size()==0 && in.good())
		{
			std::getline(in, lastline);
			line_number++;
			column = 0;
			newline_done = false;
		}
		size_t nblank = lastline.find_first_not_of(" \t\r");
		if (nblank != std::string::npos)
		{
			int indent = 0;
			for (int i=0; i<nblank; ++i)
			{
				if (lastline[i]=='\t')
				{
					indent = indent+(8-indent%8);
				}
				else
				{
					indent++;
				}
			}
			column = indent;
			if (column != columns.back())
			{
				if (column > columns.back())
				{
					columns.push_back(column);
					inferred = Token(TT_INDENT);
					return true;
				}
				if (column < columns.back())
				{
					auto instack = std::find(columns.begin(), columns.end(), column);
					if (instack == columns.end())
					{
						throw Error(line_number,"Bad indent");
					}
					columns.pop_back();
					inferred = Token(TT_OUTDENT);
					return true;
				}
			}
			lastline.erase(0,nblank);
			if (lastline.size()>1 && (lastline[0]=='-' && lastline[1]=='-'))
			{
				// If there is a third character,
				// and the three characters would make a legal 'symbol' token, then
				// the two dashes do not start a comment, except of course that
				// three dashes would start a comment.
				if (lastline.size()<3 || lastline[2]=='-' || !ascSymbol(lastline[2]))
				{
					lastline.clear();
					continue;
				}
			}
			break;
		}
		else
		{
			lastline.clear();
		}
	}
	return false;
}
/*
 * This system doesn't guess anything.
 * No blocks with indent-dedent.
 * No semicolons except the ones in the text.
 */
bool ExplicitBlocking(istream& in, Token&)
{
	while (in.good())
	{
		while (lastline.size()==0 && in.good())
		{
			std::getline(in, lastline);
			line_number++;
		}
		size_t nblank = lastline.find_first_not_of(" \t\r");
		if (nblank != std::string::npos)
		{
			lastline.erase(0,nblank);
			if (lastline.size()>1 && (lastline[0]=='-' && lastline[1]=='-'))
			{
				// If there is a third character,
				// and the three characters would make a legal 'symbol' token, then
				// the two dashes do not start a comment, except of course that
				// three dashes would start a comment.
				if (lastline.size()<3 || lastline[2]=='-' || !ascSymbol(lastline[2]))
				{
					lastline.clear();
					continue;
				}
			}
			break;
		}
		else
		{
			lastline.clear();
		}
	}
	return false;
}
/*
 * This system infers semicolons. There are basically two rules.
 * One rule is that if the last token on the line is not a semicolon,
 * then one is needed and should be inferred.
 * Second rule is, don't infer a semicolon if a left paren has not been
 * matched with a right paren.
 * Additionally, blank lines after a semicolon don't call for inferred
 * semicolons.
 * This system may do things redundantly. The alternative would be
 * to use a token buffer to help insert the inferred tokens.
 */
bool InferSemicolons(istream& in, Token& inferred)
{
	/*
	 * If we run out of characters in the line, then it
	 * may be time for a semicolon. If the last token off
	 * the line was a semicolon, then one is not needed.
	 */
	enum { BLANK, NEED_SEMI, HAD_SEMI };
	static int semi_status = BLANK;
	static int parens = 0;
	//cout << "Entry semi_status = " << (semi_status==BLANK?"BLANK":(semi_status==NEED_SEMI?"NEED_SEMI":"HAD_SEMI")) << endl;
	while (in.good())
	{
		while (lastline.size()==0 && in.good())
		{
			//cout << "EOL semi_status = " << (semi_status==BLANK?"BLANK":(semi_status==NEED_SEMI?"NEED_SEMI":"HAD_SEMI")) << endl;
			if (semi_status == NEED_SEMI && parens==0)
			{
				inferred = Token(TT_SEMI);
				semi_status = HAD_SEMI;
				return true;
			}
			std::getline(in, lastline);
			semi_status = BLANK;
			line_number++;
		}
		size_t nblank = lastline.find_first_not_of(" \t\r");
		if (nblank != std::string::npos)
		{
			lastline.erase(0,nblank);
			if (lastline.size()>1 && (lastline[0]=='-' && lastline[1]=='-'))
			{
				// If there is a third character,
				// and the three characters would make a legal 'symbol' token, then
				// the two dashes do not start a comment, except of course that
				// three dashes would start a comment.
				if (lastline.size()<3 || lastline[2]=='-' || !ascSymbol(lastline[2]))
				{
					lastline.clear();
					//cout << "comment semi_status = " << (semi_status==BLANK?"BLANK":(semi_status==NEED_SEMI?"NEED_SEMI":"HAD_SEMI")) << endl;
					if (semi_status == NEED_SEMI && parens==0)
					{
						inferred = Token(TT_SEMI);
						return true;
					}
					continue;
				}
			}
			//cout << "Non-EOL (" << lastline << ") semi_status = " << (semi_status==BLANK?"BLANK":(semi_status==NEED_SEMI?"NEED_SEMI":"HAD_SEMI")) << endl;
			break;
		}
		else
		{
			lastline.clear();
			//cout << "EOL semi_status = " << (semi_status==BLANK?"BLANK":(semi_status==NEED_SEMI?"NEED_SEMI":"HAD_SEMI")) << endl;
			if (in.good() && semi_status == NEED_SEMI && parens==0)
			{
				inferred = Token(TT_SEMI);
				return true;
			}
		}
	}
	if (lastline[0]==';' && parens==0)
		semi_status = HAD_SEMI;
	else if (lastline[0]=='(')
	{
		parens++;
		semi_status = NEED_SEMI;
	}
	else if (lastline[0]==')')
	{
		parens--;
		semi_status = NEED_SEMI;
	}
	else
		semi_status = NEED_SEMI;
	return false;
}
Token getToken(istream& in)
{
	static bool newline_done = false;

	Token inferred;
#if 1
	if (InferSemicolons(in, inferred))
		return inferred;
#elif 1
	if (IndentDedentHandling(in, inferred))
		return inferred;
#else
	if (ExplicitBlocking(in, inferred))
		return inferred;
#endif
	if (!in.good())
		return Token(TT_EOF);
	if (isalpha(lastline[0]) || lastline[0]=='_')
	{
		//
		// I am dividing the names into variable names and constructor
		// names because in the future there will be constructors.
		// I am not implementing qualified names because I'm not
		// implementing modules.
		//
		size_t nname = lastline.find_first_not_of("abcdefghijklmnopqurstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789'",1);
		Token id;
		if (isupper(lastline[0]))
			id = Token(TT_CONID);
		else
			id = Token(TT_VARID);
		id.text = lastline.substr(0,nname);
		if (id.text == "data")
			id = Token(TT_DATA);
		lastline.erase(0,nname);
		return id;
	}
	if (isdigit(lastline[0]))
	{
		size_t ndigits = lastline.find_first_not_of("0123456789",1);
		Token num;
		if (lastline[ndigits]=='.' && isdigit(lastline[ndigits+1]))
		{
			num = Token(TT_DOUBLE);
			ndigits = lastline.find_first_not_of("0123456789", ndigits+1);
			num.dval = atof(lastline.c_str());
		}
		else
		{
			num = Token(TT_INTEGER);
			num.ival = atoi(lastline.c_str());
		}
		lastline.erase(0,ndigits);
		return num;
	}
	if (lastline[0]=='"')
	{
		// Might be hokey.
		// Not de-escaping the characters ?
		// Not letting string cross to next line?
		size_t k=0;
		for (k=1; k<lastline.length(); ++k)
		{
			if (lastline[k]=='"')
				break;
			if (lastline[k]=='\\')
				++k;
		}
		Token str(TT_STRING);
		str.text = lastline.substr(1,k-1);
		lastline.erase(0,k+1);
		return str;
	}
	if (lastline[0]==';')
	{
		lastline.erase(0,1);
		return Token(TT_SEMI);
	}
	if (lastline[0]=='(')
	{
		Token paren(TT_LPAREN);
		paren.text = lastline.substr(0,1);
		lastline.erase(0,1);
		return paren;
	}
	if (lastline[0]==')')
	{
		Token paren(TT_RPAREN);
		paren.text = lastline.substr(0,1);
		lastline.erase(0,1);
		return paren;
	}
	if (string(",[]`{}").find(lastline[0])!=string::npos)
	{
		// This group along with "();", is special because they are
		// single-character tokens, unlike the ones below which a token
		// can have a string of.
		// Commas are used to make lists and tuples.
		// Backticks are used to turn a function name into a binary
		// operator, which would matter if the syntax level has operator
		// precedence instead of prefix notation. (see varop in Haskell)
		// {} are used to bracket blocks of declarations. We're not parsing
		// such things right now.
		Token oper(TT_OPER);
		oper.text = lastline.substr(0,1);
		lastline.erase(0,1);
		return oper;
	}
	// This bunch of characters consists of characters that can occur
	// in sequences of any length. A few of the sequences are reserved,
	// such as "->" and "..". "--" Is detected above and is used for the
	// non-nested comment.
	size_t nchrs = lastline.find_first_not_of("!#$%&*+./<=>?@\\^|-~:");
	if (nchrs != 0)
	{
		Token oper(TT_OPER);
		oper.text = lastline.substr(0,nchrs);
		lastline.erase(0,nchrs);
		return oper;
	}
	throw Error(line_number, string("bad chrs ")+lastline);
}
struct Parser {
	Token token;
	void next(istream& in)
	{
		token = getToken(in);
	}
};
struct Node {
	virtual ostream& print(ostream& out) const { return out; }
	virtual int output_computation(ostream& out, int n, const vector<string>& env) { return n; }
	virtual ~Node() {}
};
ostream& operator<<(ostream& out, const Node& node)
{
	return node.print(out);
}
struct Num : Node {
	Num(int ivalue) : value(ivalue) {}
	Num(double dvalue) : value(dvalue) {}
	ostream& print(ostream& out) const { return out << value; }
	int output_computation(ostream& out, int n, const vector<string>& env);
	double value;
};
struct Var : Node {
	Var(const string& id) : id(id) {}
	ostream& print(ostream& out) const { return out << id; }
	int output_computation(ostream& out, int n, const vector<string>& env);
	string id;
};
struct Str : Node {
	Str(const string& text) : text(text) {}
	ostream& print(ostream& out) const { return out << '"' <<  text << '"'; }
	int output_computation(ostream& out, int n, const vector<string>& env);
	string text;
};
struct Operator : Node {
	Operator(const string& id) : id(id) {}
	string id;
	ostream& print(ostream& out) const { return out << id; }
};
struct Apply : Node {
	// In many implementations of such things, an Apply node
	// is a pair. But the supercombinator code will want to
	// recover the arity of the (partial at times) application,
	// so we may as well retain it.
	Node* to_apply;
	vector<Node*> arguments;
	// For printing, if any of our entities is itself an application,
	// print parens around it.
	static ostream& print_bracketed(ostream& out, const Node* term)
	{
		bool paren = dynamic_cast<const Apply*>(term) != nullptr;
		if (paren) out << '(';
		out << *term;
		if (paren) out << ')';
		return out;
	}
	int output_computation(ostream& out, int n, const vector<string>& env);
	ostream& print(ostream& out) const
	{
		print_bracketed(out, to_apply);// out << *to_apply;
		auto iarg = arguments.begin();
		for (;iarg != arguments.end();)
		{
			out << ' ';
			print_bracketed(out, *iarg); iarg++;
#if 0
			bool paren = dynamic_cast<Apply*>(*iarg) != nullptr;
			if (paren) out << '(';
			out << **iarg; iarg++;
			if (paren) out << ')';
#endif
		}
		return out;
	}
};
class Defineable {
public:
	virtual ~Defineable() {}
	virtual ostream& print(ostream& out) const = 0;
	virtual void output_function_prototype(ostream& out, const string& id) const = 0;
	virtual void output_function_info(ostream& out, const string& id) const = 0;
	virtual void output_function_definition(ostream& out, const string& id) const = 0;
};
struct Constructor {
	string constructor;
	vector <string> arguments;
	ostream& print(ostream& out) const;
	void output_function_prototype(ostream& out, const string& id) const {}
};
struct Type : public Defineable {
	vector <string> arguments;
	vector <Constructor> constructors;
	ostream& print(ostream& out) const;
	void output_function_prototype(ostream& out, const string& id) const {}
};
struct Function : public Defineable {
	vector<string> arguments;
	Node* body;
	ostream& print(ostream& out) const;
	void output_function_prototype(ostream& out, const string& id) const;
	void output_function_heading(ostream& out, const string& id) const;
	void output_function_info(ostream& out, const string& id) const;
	void output_function_definition(ostream& out, const string& id) const;
};
struct Definition {
	string id;
	Defineable* defineable;
	ostream& print(ostream& out) const;
};
ostream& operator<<(ostream& out, const Defineable& defineable)
{
	return defineable.print(out);
}
ostream& operator<<(ostream& out, const Definition& definition)
{
	return definition.print(out);
}
ostream& Definition::print(ostream& out) const
{
	out << "Define " << id;
	out << ' ' << *defineable;
	return out;
}
ostream& Function::print(ostream& out) const
{
	auto iargs = arguments.begin();
	while (iargs != arguments.end())
		out << ' ' << *iargs++;
	out << '=';
	out << *body;
	return out;
}
ostream& Type::print(ostream& out) const
{
	return out;
}
void Function::output_function_heading(ostream& out, const string& id) const
{
	out << "void fun_" << id << ' ';
	out << "(comp_t** result, comp_t** args) /*";
	for (auto arg : arguments)
		out << arg << " ";
	out << "*/";
}
void Function::output_function_info(ostream& out, const string& id) const
{
	out << "comp_t sc_" << id << " = { fun_" << id << ", " << arguments.size() << "};" << endl;
}
void Function::output_function_prototype(ostream& out, const string& id) const
{
	output_function_heading(out, id);
	out << ';' << endl;
}
int Var::output_computation(ostream& out, int n, const vector<string>& env)
{
	auto lookup = find(env.begin(), env.end(), id);
	if (lookup != env.end())
	{
		auto index = distance(env.begin(), lookup);
		out << "    comp_t *e" << n << " = args[" << index << "]; /* " << id << "*/" << endl;
	}
	else
	{
		out << "    comp_t *e" << n << " = &sc_" << id << "; /* " << id << "*/" <<endl;
	}
	return n;
}
int Str::output_computation(ostream& out, int n, const vector<string>& env)
{
	out << "    comp_t *e" << n << " = str(\"" << text << "\");" << endl;
	return n;
}
int Num::output_computation(ostream& out, int n, const vector<string>& env)
{
	out << "    *e" << n << " = num(" << value << ");" << endl;
	return n;
}
int Apply::output_computation(ostream& out, int n, const vector<string>& env)
{
	vector<int> comps;
	for (int i=0; i<arguments.size(); ++i)
	{
		n = arguments[i]->output_computation(out, n, env);
		comps.push_back(n);
		++n;
	}
	int nfunc = to_apply->output_computation(out, n, env);
	out << "    *e" << n+1 << " = app(e" << nfunc << ", " << arguments.size();
	for (auto comp : comps)
		out << ", e" << comp;
	out << ");" << endl;
	return nfunc+1;
}
void Function::output_function_definition(ostream& out, const string& id) const
{
	output_function_heading(out, id);
	out << "{" << endl;
	int n = body->output_computation(out, 0, arguments);
	out << "    *result = e" << n << ";" << endl;
	out << "}" << endl;
}
Var* parse_var(Parser& parser, istream& in)
{
	if (parser.token.type == TT_VARID)
	{
		Var* var = new Var(parser.token.text);
		parser.next(in);
		return var;
	}
	return nullptr;
}
Node* parse_con(Parser& parser, istream& in)
{
	return nullptr; // we know what they look like but can't use constructors
}
Node* parse_literal(Parser& parser, istream& in)
{
	if (parser.token.type == TT_INTEGER)
	{
		Num* num = new Num(parser.token.ival);
		parser.next(in);
		return num;
	}
	if (parser.token.type == TT_DOUBLE)
	{
		Num* num = new Num(parser.token.dval);
		parser.next(in);
		return num;
	}
	if (parser.token.type == TT_STRING)
	{
		Str* str = new Str(parser.token.text);
		parser.next(in);
		return str;
	}
	// This is where character and string literals go
	return nullptr;
}
Node* parse_oper(Parser& parser, istream& in)
{
	if (parser.token.type == TT_OPER)
	{
		Operator* oper = new Operator(parser.token.text);
		parser.next(in);
		return oper;
	}
	return nullptr;
}
Var* parse_apat(Parser& parser, istream& in)
{
	return parse_var(parser,in);
}
/*
 * Let's go to the Haskell reference to be consistent
 * and avoid unnecessary deviation.
 *
 * topdecl = data simpletype [ '=' constrs][deriving]
 *         | decl
 * simpletype = tycon tyvar*
 * constrs = constr ('|' constr)*
 * constr = con atype*
 *          | btype conop btype
 *          | con '{' fielddecl* '}'
 * fielddecl = vars '::' type
 * tycon = conid
 * tyvar = varid
 * btype = btype? atype
 * atype = gtycon | tyvar
 * gtycon = qtycon
 * qtycon = qual? tycon
 * conop = consym | `conid`
 * decl = funlhs rhs
 * funlhs = var apat+
 *        | pat varop pat
 *        | '(' funlhs ')' apat+
 * rhs = '=' exp whereclause?
 *       | gdrhs whereclause?
 * whereclause = 'where' decls
 * exp = infixexp
 * infixexp = lexp qop infixexp
 *          | '-' infixexp
 *          | lexp
 * lexp = 'let' decls 'in' exp
 *        | 'if' exp ';'? 'then' exp ';'? else exp
 *        | 'case' exp 'of' '{' alts '}' # where the brackets could be from layout
 *        | fexp
 * fexp = fexp? aexp
 * aexp = qvar
 *        | gcon
 *        | literal
 *        | '(' exp ')'
 *        # This is where syntax sugar for tuples and lists goes too.
 * qvar = qvarid
 * qvarid = qual? varid
 * qual = modid '.' # That is, a qualifier is a string of conids with periods. Prelude.Data.
 * modid = qual* conid
 *
 * One form of expression that conforms to the above and is easier to parse is
 * exp = apply
 *      | var
 *      | literal
 *      | '(' exp ')'
 * apply = var | '(' exp ')' exp*
 */
Node* parse_expr(Parser& parser, istream& in);
Node* parse_primary(Parser& parser, istream& in)
{
	Node* primary;
	if (parser.token.type == TT_LPAREN)
	{
		parser.next(in);
		primary = parse_expr(parser,in);
		if (parser.token.type != TT_RPAREN)
			throw Error(line_number,"left '(' not matched");
		parser.next(in);
	}
	else
		primary = parse_var(parser,in);
	if (!primary) // an application could be an id but isn't always
		primary = parse_con(parser,in);
	if (!primary)
		primary = parse_oper(parser,in);
	if (!primary)
		throw Error(line_number,"Expected variable, constructor or operator");
	return primary;
}
Node* parse_expr(Parser& parser, istream& in)
{
	Node* literal = parse_literal(parser, in);
	if (literal)
		return literal; // A literal cannot be applied as a function
	Node* apply = parse_primary(parser,in);
	vector<Node*> arguments;
	while (parser.token.type != TT_SEMI && parser.token.type != TT_RPAREN && parser.token.type != TT_EOF)
	{
		Node* argument = parse_literal(parser,in);
		if (!argument)
			argument = parse_primary(parser,in);
		//cout << "An argument " << *argument << endl;
		if (argument)
			arguments.push_back(argument);
	}
	if (arguments.size()==0)
		return apply;
	Apply* application = new Apply();
	application->to_apply = apply;
	application->arguments = arguments;
	return application;
}
Definition* parse_definition(Parser& parser, istream& in)
{
	Var* name = parse_var(parser,in);
	if (name == nullptr)
		throw Error(line_number,"expecting varid");
	Definition* definition = new Definition();
	definition->id = name->id;
	Function* function = new Function();
	//cout << "after name " << parser.token.text << endl;
	while (parser.token.text != "=")
	{
		Var* arg = parse_apat(parser,in);
		function->arguments.push_back(arg->id);
	}
	parser.next(in);
	Node* expr = parse_expr(parser, in);
	if (parser.token.type != TT_SEMI)
		throw Error(line_number,"semicolon expected");
	parser.next(in);
	function->body = expr;
	definition->defineable = function;
	cout << "Defined: " << definition->id << endl;
	return definition;
}
typedef list<Definition*> Definitions;
Definitions parse_definitions(Parser& parser, istream& in)
{
	Definitions definitions;
	while (parser.token.type != TT_EOF)
	{
		Definition* definition = parse_definition(parser, in);
		definitions.push_back(definition);
	}

	return definitions;
}
void output_function_prototype(ostream& out, Definition& definition)
{
	definition.defineable->output_function_prototype(out, definition.id);
}
void output_function_info(ostream& out, Definition& definition)
{
	definition.defineable->output_function_info(out, definition.id);
}
void output_function_definition(ostream& out, Definition& definition)
{
	definition.defineable->output_function_definition(out, definition.id);
}
void output_code(Definitions& definitions)
{
	// output function prototypes
	// output info for each function
	// output function definitions
	for (auto definition: definitions)
		output_function_prototype(cout, *definition);
	for (auto definition: definitions)
		output_function_info(cout, *definition);
	for (auto definition: definitions)
		output_function_definition(cout, *definition);
}
int main(int argc, char** argv)
{
	const char* input = argv[1];
	bool tokenTest = false;
	bool showDefinitions = false;
	for (int i = 1; i<argc; ++i)
	{
		if (strcmp(argv[i],"--tokenTest")==0)
			tokenTest = true;
		else if (strcmp(argv[i],"--showDefinitions")==0)
			showDefinitions = true;
		else if (argv[i][0]=='-' && argv[i][1]=='-')
			continue;
		else
			input = argv[i];
	}
	std::fstream file;
	file.open(input, std::ios::binary | std::fstream::in);
	Parser parser;

	try {
		if (tokenTest) {
			do {
				parser.next(file);
				cout << parser.token << endl;
			} while (parser.token.type != TT_EOF);
		} else {
			parser.next(file);
			Definitions definitions = parse_definitions(parser, file);
			if (showDefinitions)
				for (auto definition : definitions)
					cout << *definition << endl;
			output_code(definitions);
		}
	}
	catch (const Error& error)
	{
		cout << "Exception " << error.msg << " on line " << error.line << endl;
	}
	catch (const char* exception)
	{
		cout << "Exception: "<< exception << endl;
	}
}
