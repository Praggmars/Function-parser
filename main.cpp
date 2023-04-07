#include "parser.h"

int main()
{
	FunctionParser parser;

	try
	{
		//parser.Parse("sin(z6+5*c+i)-z*z+c");
		parser.Parse("z*z+c");
		std::cout << *parser.PseudoCode() << std::endl;

		FunctionEvaluator<std::complex<double>> eval(parser);
		for (auto& v : eval.Variables())
			v.second = std::complex<double>(0.5, 0.0);
		std::cout << eval() << std::endl;
	}
	catch (const std::exception& ex)
	{
		std::cout << "Error: " << ex.what() << std::endl;
	}	

	return 0;
}