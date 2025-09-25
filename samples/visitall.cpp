#include <edat.h>
#include <parsers.h>

namespace fs = std::filesystem;

void printContents(const edat::Table& tbl)
{
    printf("All integers:\n");
    tbl.getAll<int>([&](const std::string& name, int val) { printf("\t%s: %d\n", name.c_str(), val); } );
    printf("All floats:\n");
    tbl.getAll<float>([&](const std::string& name, float val) { printf("\t%s: %f\n", name.c_str(), val); } );
    printf("All float[]:\n");
    tbl.getAll<std::vector<float>>([&](const std::string& name, const std::vector<float>& val)
    {
        printf("\t%s: [", name.c_str());
        for (float f : val)
            printf("%f, ", f);
        printf("]\n");
    });
    printf("All strings:\n");
    tbl.getAll<std::string>([&](const std::string& name, const std::string& val) { printf("\t%s: '%s'\n", name.c_str(), val.c_str()); } );
    printf("All tables:\n");
    tbl.getAll<edat::Table>([&](const std::string& name, const edat::Table& tbl) { printContents(tbl); });
}

int main(int argc, const char** argv)
{
    edat::Table tbl;
    tbl.set("first", 10.f);
    tbl.set("second", 20.f);
    tbl.set("third", 1);
    tbl.set<std::string>("forth", "forth?");
    tbl.set("fifth", 30.f);

    // Visit all floats
    tbl.getAll<float>([&](const std::string& name, float val) { printf("%s: %.2f\n", name.c_str(), val); } );

    // getOr
    printf("third: %d\n", tbl.getOr<int>("third", 20));

    // get
    tbl.get<float>("fifth", [&](float val) { printf("fifth is %.2f\n", val); } );

    edat::ParserSuite psuite;
    psuite.addLambdaParser<int>("int", [](const std::string_view& str) -> int
    {
        return std::stoi(std::string(str));
    });
    psuite.addLambdaParser<float>("float", [](const std::string_view& str) -> float
    {
        return std::stof(std::string(str));
    });
    psuite.addLambdaParser<std::string>("str", [](const std::string_view& str) -> std::string
    {
        return std::string(str);
    });

    printf("\n\nParsing in-memory string\n\n");
    std::string fileContents =  " something : float = \"-2\"\n"
                                "anotherThing:float = \"42.123\"\n"
                                "Yet123Another456Thing___:int = \"77\"\n"
                                "SomeQuotedNumber:float = \"-4.768\"\n"
                                "ScientificNumber  :   float = \"1e5\"";

    edat::Table res = edat::parseString(fileContents, psuite);
    printContents(res);

    printf("\n\nReading simple.edat from file\n");

    fs::path simple = "simple.edat";
    fs::path fullPath = fs::current_path() / simple;

    edat::Table fres = edat::parseFile(fullPath, psuite);
    printContents(fres);

    return 0;
}

