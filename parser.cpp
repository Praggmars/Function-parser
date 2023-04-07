#include "parser.h"
#include <cstring>
#include <sstream>
#include <vector>

static const char* const g_FunctionNames[] = {
	"sin", "cos", "tan", "sinh", "cosh", "tanh", "exp", "log", "abs", "pos", "ang", "re", "im"
};

FuncParseExcept::FuncParseExcept(const ErrorType error, const size_t where) :
	m_foundError(error),
	m_errorOffset(where)
{
	static const char* const s_ErrorNameTable[] = {
		"No input",
		"Unexpected symbol",
		"Unknown symbol",
		"Open braces",
		"Operator expected",
		"Empty function",
		"Dangling operator",
		"Invalid variable index",
		"Unknown error"
	};
	switch (error)
	{
	case NoInput:
	case EmptyFunction:
	case UnknownError:
		m_readableError = s_ErrorNameTable[error];
		break;
	default:
	{
		std::stringstream ss;
		ss << s_ErrorNameTable[error] << " at position " << (where + 1);
		m_readableError = ss.str();
		break;
	}
	}
}

const char* FuncParseExcept::what() const noexcept
{
	return m_readableError.c_str();
}

FunctionParser::FuncElem::FuncElem(const Type type) :
	type(type) {}

FunctionParser::Variable::Variable(const int index) :
	FuncElem(Type::Variable),
	index(index) {}

void FunctionParser::Variable::Print(std::ostream& os) const
{
	os << (index < 0 ? 'c' : 'z');
	if (index > 0)
		os << index;
}

FunctionParser::Constant::Constant(const std::complex<double> value) :
	FuncElem(Type::Constant),
	value(value) {}

void FunctionParser::Constant::Print(std::ostream& os) const
{
	os << value;
}

FunctionParser::Function::Function(const Name funcName) :
	name(funcName),
	FuncElem(Type::Function),
	param() {}

void FunctionParser::Function::Print(std::ostream& os) const
{
	os << g_FunctionNames[static_cast<size_t>(name)] << '(';
	if (param)
		os << (*param);
	os << ')';
}

FunctionParser::Operator::Operator(const Name funcName, const int precedence) :
	FuncElem(Type::Operator),
	name(funcName),
	params{},
	precedence(precedence) {}

void FunctionParser::Operator::Print(std::ostream& os) const
{
	const char* const opNames[] = {
		"add", "sub", "mul", "div", "pow"
	};
	os << opNames[static_cast<size_t>(name)] << '(';
	if (params[0])
		os << (*params[0]);
	os << ',';
	if (params[1])
		os << (*params[1]);
	os << ')';
}

static size_t CountBetweenBraces(const char* const func, const size_t offset, const size_t length)
{
	size_t braceCntr = 1;
	for (size_t j = offset + 1; j < offset + length; j++)
	{
		if ('(' == func[j])
		{
			braceCntr++;
		}
		else if (')' == func[j])
		{
			if (0 == (--braceCntr))
			{
				return j - offset;
			}
		}
	}
	throw FuncParseExcept(FuncParseExcept::OpenBraces, offset);
}

static bool IsLetter(const char ch)
{
	return
		(ch >= 'a' && ch <= 'z') ||
		(ch >= 'A' && ch <= 'Z');
}
static bool IsDigit(const char ch)
{
	return ch >= '0' && ch <= '9';
}
static bool IsNamePart(const char ch)
{
	return
		IsLetter(ch) ||
		IsDigit(ch) ||
		'_' == ch;
}
static bool IsNumberPart(const char ch)
{
	return
		IsDigit(ch) ||
		ch == '.' ||
		ch == '-';
}
static size_t CountNameLength(const char* const name, const size_t length)
{
	for (size_t i = 0; i < length; i++)
		if (!IsNamePart(name[i]))
			return i;
	return length;
}
static double ScanNumber(const char* const func, size_t& offset)
{
	double fractionalDiv = 1.0;
	bool digitPresent = false;
	double num = 0.0;
	const size_t firstIdx = offset;
	if (func[firstIdx] == '-')
		offset++;
	while (IsDigit(func[offset]))
	{
		num = 10.0 * num + static_cast<double>(func[offset++] - '0');
		digitPresent = true;
	}
	if (func[offset] == '.')
	{
		while (IsDigit(func[++offset]))
		{
			num += static_cast<double>(func[offset] - '0') / (fractionalDiv /= 10.0);
			digitPresent = true;
		}
	}
	if (!digitPresent)
		throw FuncParseExcept(FuncParseExcept::UnexpectedSymbol, firstIdx);
	return func[firstIdx] == '-' ? -num : num;
}

FunctionParser::Function::Name FunctionParser::GetFunctionNameApplyPrecision(const std::string& name, const size_t offset)
{
	for (size_t i = 0; i < _countof(g_FunctionNames); i++)
	{
		if (g_FunctionNames[i] == name)
		{
			Function::Name n = static_cast<Function::Name>(i);
			if (m_supportedPrecision != Precision::Single)
			{
				switch (n)
				{
				case Function::Name::pos:
				case Function::Name::re:
				case Function::Name::im:
					break;
				default:
					m_supportedPrecision = Precision::Single;
					break;
				}
			}
			return n;
		}
	}
	throw FuncParseExcept(FuncParseExcept::UnknownSymbol, offset);
}

std::unique_ptr<FunctionParser::FuncElem> FunctionParser::ScanEvaluated(const char* const func, size_t& offset, const size_t length)
{
	if ('(' == func[offset])
	{
		const size_t bracedLength = CountBetweenBraces(func, ++offset, length);
		std::unique_ptr<FuncElem> funcElem = ParsePart(func, offset, bracedLength);
		offset += bracedLength + 1;
		return funcElem;
	}
	if (IsLetter(func[offset]))
	{
		const size_t nameLength = CountNameLength(func + offset, length - offset);
		std::unique_ptr<FuncElem> funcElem;
		const std::string name(func + offset, nameLength);
		if (func[offset += nameLength] == '(')
		{
			funcElem = std::make_unique<Function>(GetFunctionNameApplyPrecision(name, offset));
			static_cast<Function*>(funcElem.get())->param = ScanEvaluated(func, offset, length);
		}
		else
		{
			if (name == "i")
				funcElem = std::make_unique<Constant>(std::complex<double>(0.0, 1.0));
			else if (name == "c")
			{
				funcElem = std::make_unique<Variable>(-1);
				m_usedVariables.insert(-1);
			}
			else if (name[0] == 'z')
			{
				if ((name.length() > 1 && name[1]== '0') || name.length() > 10)
					throw FuncParseExcept(FuncParseExcept::UnexpectedSymbol, offset - name.length());
				int index = 0;
				for (size_t i = 1; i < name.length(); i++)
				{
					if (!IsDigit(name[i]))
						throw FuncParseExcept(FuncParseExcept::UnexpectedSymbol, offset - name.length());
					index = index * 10 + (name[i] - '0');
				}

				funcElem = std::make_unique<Variable>(index);
				m_usedVariables.insert(index);
			}
			else
				throw FuncParseExcept(FuncParseExcept::UnexpectedSymbol, offset + 1 - name.length());
		}
		return funcElem;
	}
	if (IsNumberPart(func[offset]))
	{
		return std::make_unique<Constant>(ScanNumber(func, offset));
	}
	throw FuncParseExcept(FuncParseExcept::UnknownSymbol, offset);
}

std::unique_ptr<FunctionParser::FuncElem> FunctionParser::ScanOperator(const char* const func, size_t& offset) const
{
	switch (func[offset++])
	{
	case '+': return std::make_unique<FunctionParser::Operator>(FunctionParser::Operator::Name::add, 0);
	case '-': return std::make_unique<FunctionParser::Operator>(FunctionParser::Operator::Name::sub, 0);
	case '*': return std::make_unique<FunctionParser::Operator>(FunctionParser::Operator::Name::mul, 1);
	case '/': return std::make_unique<FunctionParser::Operator>(FunctionParser::Operator::Name::div, 1);
	case '^': return std::make_unique<FunctionParser::Operator>(FunctionParser::Operator::Name::pow, 2);
	default: throw FuncParseExcept(FuncParseExcept::OperatorExpected, offset - 1);
	}
}

std::unique_ptr<FunctionParser::FuncElem> FunctionParser::ParsePart(const char* const func, const size_t offset, const size_t length)
{
	if (!func || !length)
		throw FuncParseExcept(FuncParseExcept::NoInput, 0);

	std::vector<std::unique_ptr<FuncElem>> tokenized;
	bool scanEvaluated = true;

	for (size_t i = offset; i < offset + length; scanEvaluated = !scanEvaluated)
		tokenized.emplace_back(scanEvaluated ? ScanEvaluated(func, i, length + offset - i) : ScanOperator(func, i));
	if (scanEvaluated)
		throw FuncParseExcept(FuncParseExcept::DanglingOperator, offset + length - 1);
	if (tokenized.empty())
		throw FuncParseExcept(FuncParseExcept::NoInput, 0);

	size_t tokenCount = tokenized.size();
	for (int precedence = 2; precedence >= 0; precedence--)
	{
		size_t storeIdx = 0, readIdx = 0;
		while (readIdx < tokenCount)
		{
			if ((FuncElem::Type::Operator == tokenized[readIdx]->type) &&
				(static_cast<Operator*>(tokenized[readIdx].get())->precedence == precedence))
			{
				if (!(readIdx + 1 < tokenCount && storeIdx > 0))
					throw FuncParseExcept(FuncParseExcept::EmptyFunction, 0);
				Operator* op = static_cast<Operator*>(tokenized[readIdx].get());
				op->params[0] = std::move(tokenized[storeIdx - 1]);
				op->params[1] = std::move(tokenized[readIdx + 1]);
				tokenized[storeIdx - 1] = std::move(tokenized[readIdx]);
				readIdx += 2;
			}
			else
			{
				if (storeIdx != readIdx)
					tokenized[storeIdx] = std::move(tokenized[readIdx]);
				readIdx++;
				storeIdx++;
			}
		}

		tokenCount = storeIdx;
	}

	if (1 != tokenCount)
		throw FuncParseExcept(FuncParseExcept::UnknownError, 0);

	return std::move(tokenized[0]);
}

FunctionParser::FunctionParser() : m_supportedPrecision(Precision::Extended) {}

FunctionParser::FunctionParser(const char* const function) : FunctionParser()
{
	Parse(function);
}

FunctionParser::~FunctionParser()
{
	Clear();
}

void FunctionParser::Parse(const char* const function)
{
	std::string input;
	Clear();
	for (size_t i = 0; function[i]; i++)
		if (!std::isspace(function[i]))
			input += function[i];
	std::unique_ptr<FuncElem> output = ParsePart(input.c_str(), 0, input.length());
	m_inputFunc = std::move(input);
	m_parsedFunc = std::move(output);
}

void FunctionParser::Clear()
{
	m_inputFunc.clear();
	m_parsedFunc.reset();
	m_usedVariables.clear();
	m_supportedPrecision = Precision::Extended;
}

std::ostream& operator<<(std::ostream& os, const FunctionParser::FuncElem& funcElem)
{
	funcElem.Print(os);
	return os;
}