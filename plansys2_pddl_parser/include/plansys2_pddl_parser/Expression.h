
#pragma once

#include <plansys2_msgs/shared_ptr_typedefs.h>

#include <plansys2_msgs/Node.h>
#include <plansys2_msgs/Tree.h>

#include <plansys2_pddl_parser/Condition.h>

namespace parser { namespace pddl {

class Instance;

class Expression : public Condition {

public:

	virtual ~Expression() {}
	virtual std::string info() const = 0;
	virtual double evaluate() = 0;
	virtual double evaluate( Instance & ins, const StringVec & par ) = 0;
	virtual IntSet params() = 0;

	// inherit
	virtual void print( std::ostream & stream ) const {
		stream << info();
	}

	virtual void parse( Stringreader & f, TokenStruct< std::string > & ts, Domain & d ) {}
	virtual void addParams( int m, unsigned n ) {}
};

Expression * createExpression( Stringreader & f, TokenStruct< std::string > & ts, Domain & d );

class CompositeExpression : public Expression {

public:

	std::string op;
	Expression * left;
	Expression * right;

	CompositeExpression( const std::string& c ) : op( c ) {}

	CompositeExpression( const std::string& c, Expression * l, Expression * r ) : op( c ), left( l ), right( r ) {}

	~CompositeExpression() {
		delete left;
		delete right;
	}

	void parse( Stringreader & f, TokenStruct< std::string > & ts, Domain & d ) {
		f.next();
		left = createExpression( f, ts, d );
		right = createExpression( f, ts, d );
		f.next();
		f.assert_token( ")" );
	}

	std::string info() const {
		std::ostringstream os;
		os << "(" << op << " " << left->info() << " " << right->info() << ")";
		return os.str();
	}

	void PDDLPrint( std::ostream & s, unsigned indent, const TokenStruct< std::string > & ts, const Domain & d ) const override {
  tabindent(s, indent);
		s << "( " << op << " ";
		left->PDDLPrint( s, indent, ts, d );
		s << " ";
		right->PDDLPrint( s, indent, ts, d );
		s << " )";
	}

	plansys2_msgs::NodeSharedPtr getTree( plansys2_msgs::Tree & tree, const Domain & d, const std::vector<std::string> & replace = {} ) const override {
		plansys2_msgs::NodeSharedPtr node = std::make_shared<plansys2_msgs::Node>();
		node->node_type = plansys2_msgs::Node::EXPRESSION;
		node->expression_type = getExprType(op);
		node->node_id = tree.nodes.size();
		tree.nodes.push_back(*node);

		plansys2_msgs::NodeSharedPtr left_child = left->getTree(tree, d, replace);
		tree.nodes[node->node_id].children.push_back(left_child->node_id);

		plansys2_msgs::NodeSharedPtr right_child = right->getTree(tree, d, replace);
		tree.nodes[node->node_id].children.push_back(right_child->node_id);

		return node;
	}

	double compute( double x, double y ) {
		double res = 0;

		if ( op == "+" ) res = x + y;
		else if ( op == "-" ) res = x - y;
		else if ( op == "*" ) res = x * y;
		else if ( op == "/" ) res = ( y == 0 ? 0 : x / y );

		return res;
	}

	double evaluate() {
		return compute( left->evaluate(), right->evaluate() );
	}

	double evaluate( Instance & ins, const StringVec & par ) {
		return compute( left->evaluate( ins, par ), right->evaluate( ins, par ) );
	}

	IntSet params() {
		IntSet lpars = left->params();
		IntSet rpars = right->params();
		lpars.insert( rpars.begin(), rpars.end() );
		return lpars;
	}

	Condition * copy( Domain & d ) {
		Expression * cleft = dynamic_cast< Expression * >( left->copy( d ) );
		Expression * cright = dynamic_cast< Expression * >( right->copy( d ) );
		return new CompositeExpression( op, cleft, cright );
	}
};

class FunctionExpression : public Expression {

public:

	ParamCond * fun;
 std::vector<Expression*> constants;

  FunctionExpression( ParamCond * c ) : fun( c ) {
    constants.clear();
    constants.resize(c->params.size());
    std::fill(constants.begin(), constants.end(), nullptr);
  }
  FunctionExpression( ParamCond * c, std::vector<Expression*> e) : fun( c ), constants(e) {}

	~FunctionExpression() {
		delete fun;
	}

	std::string info() const {
		std::ostringstream os;
		os << "(" << fun->name << fun->params << ")";
		return os.str();
	}

	void PDDLPrint( std::ostream & s, unsigned indent, const TokenStruct< std::string > & ts, const Domain & d ) const override;

	plansys2_msgs::NodeSharedPtr getTree( plansys2_msgs::Tree & tree, const Domain & d, const std::vector<std::string> & replace = {} ) const override;

	double evaluate() { return 1; }

	double evaluate( Instance & ins, const StringVec & par );

	IntSet params() {
		return IntSet( fun->params.begin(), fun->params.end() );
	}

	Condition * copy( Domain & d ) {
		return new FunctionExpression( dynamic_cast< ParamCond * >( fun->copy( d ) ) );
	}
};

class ValueExpression : public Expression {

public:

	double value;

	ValueExpression( double v ) : value( v ) {}

	std::string info() const {
		std::ostringstream os;
		os << value;
		return os.str();
	}

	void PDDLPrint( std::ostream & s, unsigned indent, const TokenStruct< std::string > & ts, const Domain & d ) const override {
		s << value;
	}

  plansys2_msgs::NodeSharedPtr getTree( plansys2_msgs::Tree & tree,
					  const Domain & d,
					  const std::vector<std::string> & replace = {} ) const override {
		plansys2_msgs::NodeSharedPtr node = std::make_shared<plansys2_msgs::Node>();
		node->node_type = plansys2_msgs::Node::NUMBER;
		node->node_id = tree.nodes.size();
		node->value = value;
		tree.nodes.push_back(*node);
		return node;
	}

	double evaluate() { return value; }

	double evaluate( Instance & ins, const StringVec & par ) {
		return value;
	}

	IntSet params() {
		return IntSet();
	}

	Condition * copy( Domain & d ) {
		return new ValueExpression( value );
	}
};

class DurationExpression : public Expression {

	void PDDLPrint( std::ostream & s, unsigned indent, const TokenStruct< std::string > & ts, const Domain & d ) const override {
		s << "?duration";
	}

	plansys2_msgs::NodeSharedPtr getTree( plansys2_msgs::Tree & tree, const Domain & d, const std::vector<std::string> & replace = {} ) const override {
		throw UnsupportedConstruct("DurationExpression");
	}

	std::string info() const {
		return "?duration";
	}

	double evaluate() {
		return -1;
	}

	double evaluate( Instance & ins, const StringVec & par ) {
		return evaluate();
	}

	IntSet params() {
		return IntSet();
	}

	Condition * copy( Domain & d ) {
		return new DurationExpression();
	}
};


class ParamExpression : public Expression {

public:

	int param;

	ParamExpression( int p ) : param( p ) {}

	std::string info() const {
		std::ostringstream os;
		os << param;
		return os.str();
	}

	void PDDLPrint( std::ostream & s, unsigned indent, const TokenStruct< std::string > & ts, const Domain & d ) const override {
		s << ts[param];
	}

	plansys2_msgs::NodeSharedPtr getTree( plansys2_msgs::Tree & tree,
						const Domain & d,
						const std::vector<std::string> & replace = {} ) const override {
		throw UnsupportedConstruct("ParamExpression");
	}

	double evaluate() { return -1; }

	double evaluate( Instance & ins, const StringVec & par ) {
  return evaluate();
	}

	IntSet params() {
		return IntSet();
	}

	Condition * copy( Domain & d ) {
		return new ParamExpression( param );
	}
};

class ConstExpression : public Expression {

public:

	int constant;
 int tid;
  ConstExpression( int c, int t ) : constant( c ), tid (t) {}

	std::string info() const {
		std::ostringstream os;
		os << constant << " " << tid;
		return os.str();
	}

	void PDDLPrint( std::ostream & s, unsigned indent, const TokenStruct< std::string > & ts, const Domain & d ) const override;

	plansys2_msgs::NodeSharedPtr getTree( plansys2_msgs::Tree & tree, const Domain & d, const std::vector<std::string> & replace = {} ) const override {
		throw UnsupportedConstruct("ConstExpression");
	}

	double evaluate() { return -1; }

	double evaluate( Instance & ins, const StringVec & par ) {
  return evaluate();
	}

	IntSet params() {
		return IntSet();
	}

	Condition * copy( Domain & d ) {
  return new ConstExpression( constant, tid );
	}
};

} } // namespaces
