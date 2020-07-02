#include <regex>
#include <fstream>

using std::ifstream;
using std::ofstream;

int main(int argc, char* argv[])
{
	if(argc<2) return 1;
	ifstream ifs;
	ifs.open(argv[1]);
	ofstream ofs;
	ofs.open(std::string(argv[1])+".convert");
	std::regex regex("(EBML_DLL extern const ebml_context )(.+)",std::regex::ECMAScript);
	
	while (ifs) {
		std::string str;
		std::getline(ifs,str);
		std::smatch base_match;
		std::regex_match(str,base_match,regex);
		if (base_match.size()) {
			str = std::string("MATROSKA_DLL extern const ebml_context ") + base_match[2].str();
			ofs <<str<<'\n';
		}
		else {
			ofs << str << '\n';
		}
	}
	ofs.close();
	ifs.close();
}



