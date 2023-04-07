#pragma once

#include <string>
#include <memory>
#include <stdexcept>
#include <set>
#include <map>
#include <iostream>
#include <complex>

namespace mth
{
	template <typename T> std::complex<T> pos(std::complex<T> t) { return std::complex<T>(std::abs(t.real()), std::abs(t.imag())); }
	template <typename T> T pos(T t) { return std::abs(t); }
	template <typename T> std::complex<T> ang(std::complex<T> t) { return std::complex<T>(std::atan2(t.imag(), t.real()), 0); }
	template <typename T> T ang(T t) { return 0; }
	template <typename T> std::complex<T> re(std::complex<T> t) { return std::complex<T>(std::abs(t.real()), 0); }
	template <typename T> T re(T t) { return t; }
	template <typename T> std::complex<T> im(std::complex<T> t) { return std::complex<T>(std::abs(t.imag()), 0); }
	template <typename T> T im(T t) { return 0; }
}

class FuncParseExcept : std::exception
{
public:
	enum ErrorType
	{
		NoInput,
		UnexpectedSymbol,
		UnknownSymbol,
		OpenBraces,
		OperatorExpected,
		EmptyFunction,
		DanglingOperator,
		InvalidVariableIndex,
		UnknownError
	};

private:
	ErrorType m_foundError;
	size_t m_errorOffset;
	std::string m_readableError;

public:
	FuncParseExcept(const ErrorType error, const size_t where);
	virtual const char* what() const noexcept override;
};

class FunctionParser
{
public:
	struct FuncElem
	{
		enum class Type
		{
			Variable,
			Constant,
			Function,
			Operator
		};
		const Type type;
		FuncElem(const Type type);
		virtual void Print(std::ostream& os) const = 0;
	};
	struct Variable : public FuncElem
	{
		int index;
		Variable(const int index);
		virtual void Print(std::ostream& os) const override;
	};
	struct Constant : public FuncElem
	{
		std::complex<double> value;
		Constant(const std::complex<double> value);
		virtual void Print(std::ostream& os) const override;
	};
	struct Function : public FuncElem
	{
		enum class Name
		{
			sin, cos, tan, sinh, cosh, tanh, exp, log, abs, pos, ang, re, im
		};

		Name name;
		std::unique_ptr<FuncElem> param;
		Function(const Name funcName);
		virtual void Print(std::ostream& os) const override;
	};
	struct Operator : public FuncElem
	{
		enum class Name
		{
			add, sub, mul, div, pow
		};
		Name name;
		std::unique_ptr<FuncElem> params[2];
		int precedence;
		Operator(const Name funcName, const int precedence);
		virtual void Print(std::ostream& os) const override;
	};

	enum class Precision
	{
		Single,
		Double,
		Extended
	};

private:
	std::set<int> m_usedVariables;
	Precision m_supportedPrecision;
	std::string m_inputFunc;
	std::unique_ptr<FuncElem> m_parsedFunc;

private:
	Function::Name GetFunctionNameApplyPrecision(const std::string& name, const size_t offset);
	std::unique_ptr<FuncElem> ScanEvaluated(const char* const func, size_t& offset, const size_t length);
	std::unique_ptr<FuncElem> ScanOperator(const char* const func, size_t& offset) const;
	std::unique_ptr<FuncElem> ParsePart(const char* const func, const size_t offset, const size_t length);

public:
	FunctionParser();
	FunctionParser(const char* const function);
	~FunctionParser();

	void Parse(const char* const function);
	void Clear();

	inline FuncElem* PseudoCode() const { return m_parsedFunc.get(); }
	inline const std::set<int>& UsedVariables() const { return m_usedVariables; }
	inline Precision SupportedPrecision() const { return m_supportedPrecision; }
};

template <typename NumberType>
class FunctionEvaluator
{
	class Elem
	{
	public:
		virtual NumberType Eval() const = 0;
	};

	class Constant : public Elem
	{
		NumberType m_value;

	public:
		Constant(const NumberType& value) : m_value(value) {}
		virtual NumberType Eval() const override
		{
			return m_value;
		}
	};

	class Variable : public Elem
	{
		const NumberType& m_value;

	public:
		Variable(const NumberType& value) : m_value(value) {}

		virtual NumberType Eval() const override
		{
			return m_value;
		}
	};

	class Function : public Elem
	{
		std::unique_ptr<Elem> m_param;
		NumberType(*m_function)(NumberType);

	public:
		Function(std::unique_ptr<Elem> param, NumberType(*function)(NumberType)) :
			m_param(std::move(param)),
			m_function(function) {}

		virtual NumberType Eval() const override
		{
			return m_function(m_param->Eval());
		}
	};

	class Operator : public Elem
	{
		std::unique_ptr<Elem> m_params[2];
		NumberType(*m_function)(NumberType, NumberType);

	public:
		Operator(std::unique_ptr<Elem> param1, std::unique_ptr<Elem> param2, NumberType(*function)(NumberType, NumberType)) :
			m_params{ std::move(param1), std::move(param2) },
			m_function(function) {}

		virtual NumberType Eval() const override
		{
			return m_function(m_params[0]->Eval(), m_params[1]->Eval());
		}
	};

private:
	std::unique_ptr<Elem> m_funcTree;
	std::map<int, NumberType> m_variables;

private:
	std::unique_ptr<Elem> ConvertOperator(const FunctionParser::Operator* op) const
	{
		NumberType(*functions[])(NumberType, NumberType) = {
			[](NumberType lhs, NumberType rhs)->NumberType {return lhs + rhs; },
			[](NumberType lhs, NumberType rhs)->NumberType {return lhs - rhs; },
			[](NumberType lhs, NumberType rhs)->NumberType {return lhs * rhs; },
			[](NumberType lhs, NumberType rhs)->NumberType {return lhs / rhs; },
			[](NumberType lhs, NumberType rhs)->NumberType {return std::pow(lhs, rhs); }
		};

		const size_t n = static_cast<size_t>(op->name);
		if (n < _countof(functions))
			return std::make_unique<Operator>(ConvertElem(op->params[0].get()), ConvertElem(op->params[1].get()), functions[n]);

		throw FuncParseExcept(FuncParseExcept::UnknownSymbol, 0);
	}
	std::unique_ptr<Elem> ConvertFunction(const FunctionParser::Function* func) const
	{
		NumberType(*functions[])(NumberType) = {
			[](NumberType p)->NumberType { return std::sin(p); },
			[](NumberType p)->NumberType { return std::cos(p); },
			[](NumberType p)->NumberType { return std::tan(p); },
			[](NumberType p)->NumberType { return std::sinh(p); },
			[](NumberType p)->NumberType { return std::cosh(p); },
			[](NumberType p)->NumberType { return std::tanh(p); },
			[](NumberType p)->NumberType { return std::exp(p); },
			[](NumberType p)->NumberType { return std::log(p); },
			[](NumberType p)->NumberType { return std::abs(p); },
			[](NumberType p)->NumberType { return mth::pos(p); },
			[](NumberType p)->NumberType { return mth::ang(p); },
			[](NumberType p)->NumberType { return mth::re(p); },
			[](NumberType p)->NumberType { return mth::im(p); }
		};

		size_t n = static_cast<size_t>(func->name);
		if (n < _countof(functions))
			return std::make_unique<Function>(ConvertElem(func->param.get()), functions[n]);

		throw FuncParseExcept(FuncParseExcept::UnknownSymbol, 0);
	}
	std::unique_ptr<Elem> ConvertConstant(const FunctionParser::Constant* funcElem) const
	{
		return std::make_unique<Constant>(funcElem->value);
	}
	std::unique_ptr<Variable> ConvertVariable(const FunctionParser::Variable* funcElem) const
	{
		const auto v = m_variables.find(funcElem->index);
		if (v == m_variables.end())
			throw FuncParseExcept(FuncParseExcept::InvalidVariableIndex, 0);

		return std::make_unique<Variable>(v->second);
	}

	std::unique_ptr<Elem> ConvertElem(const FunctionParser::FuncElem* funcElem) const
	{
		switch (funcElem->type)
		{
		case FunctionParser::FuncElem::Type::Operator:
			return ConvertOperator(static_cast<const FunctionParser::Operator*>(funcElem));
		case FunctionParser::FuncElem::Type::Function:
			return ConvertFunction(static_cast<const FunctionParser::Function*>(funcElem));
		case FunctionParser::FuncElem::Type::Constant:
			return ConvertConstant(static_cast<const FunctionParser::Constant*>(funcElem));
		case FunctionParser::FuncElem::Type::Variable:
			return ConvertVariable(static_cast<const FunctionParser::Variable*>(funcElem));
		default:
			throw FuncParseExcept(FuncParseExcept::UnknownError, 0);
		}
	}

public:
	FunctionEvaluator(const FunctionParser& parser)
	{
		for (const int v : parser.UsedVariables())
			m_variables[v] = NumberType();
		m_funcTree = ConvertElem(parser.PseudoCode());
	}
	NumberType operator()() const { return m_funcTree->Eval(); }
	inline std::map<int, NumberType>& Variables() { return m_variables; }
};

std::ostream& operator<<(std::ostream& os, const FunctionParser::FuncElem& funcElem);
