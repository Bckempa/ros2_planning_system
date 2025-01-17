
#pragma once

#include <plansys2_msgs/Node.h>
#include <plansys2_msgs/Tree.h>

#include <plansys2_pddl_parser/ParamCond.h>

namespace parser { namespace pddl {

class Task : public ParamCond {

public:

	Task() {}

	Task( const std::string & s )
		: ParamCond( s ) {}

	Task( const ParamCond * c )
		: ParamCond( c ) {}

	void PDDLPrint( std::ostream & s, unsigned indent, const TokenStruct< std::string > & ts, const Domain & d ) const override {}

	plansys2_msgs::NodeSharedPtr getTree( plansys2_msgs::Tree & tree, const Domain & d, const std::vector<std::string> & replace = {} ) const override {
		throw UnsupportedConstruct("Task");
	}

	void parse( Stringreader & f, TokenStruct< std::string > & ts, Domain & d ) {}

	void addParams( int m, unsigned n ) {}

	Condition * copy( Domain & d ) {
		return new Task( this );
	}

};

typedef std::vector< Task * > TaskVec;

} } // namespaces
