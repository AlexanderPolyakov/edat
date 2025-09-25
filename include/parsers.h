#pragma once

#include <functional>
#include <filesystem>

namespace edat
{

struct TypeParser
{
    size_t typeId; // C++ type id for validation?

    virtual ~TypeParser() {};
    virtual void parseValue(const std::string_view& name, const std::string_view& str, Table& res) const = 0;
    virtual void parseArray(const std::string_view& name, const std::vector<std::string_view>& strings, Table& res) const = 0;
};

template<typename T>
struct LambdaParser : public TypeParser
{
    std::function<T(const std::string_view&)> parseValueLambda;

    template<typename Callable>
    LambdaParser(Callable c) : parseValueLambda(c) {}
    virtual ~LambdaParser() {};

    void parseValue(const std::string_view& name, const std::string_view& str, Table& res) const final
    {
        res.set<T>(name, parseValueLambda(str));
    }
    void parseArray(const std::string_view& name, const std::vector<std::string_view>& strings, Table& res) const final
    {
        std::vector<T> arr;
        for (const std::string_view& str : strings)
            arr.push_back(parseValueLambda(str));
        res.set<std::vector<T>>(name, std::move(arr));
    }
};

struct ParserSuite
{
    std::unordered_map<std::string, TypeParser*> typeParsers;

    ~ParserSuite()
    {
        for (auto& [name, parser] : typeParsers)
            delete parser;
    }

    void addParser(const std::string_view& typeName, TypeParser* parser)
    {
        typeParsers.emplace(typeName, parser);
    }

    template<typename T, typename Callable>
    void addLambdaParser(const std::string& typeName, Callable c)
    {
        LambdaParser<T>* parser = new LambdaParser<T>(c);
        addParser(typeName, parser);
    }
};

bool isWhitespace(char ch)
{
    // We want to skip only specific stuff!
    return ch == ' ' || ch == '\t';
}

bool isLineBreak(char ch)
{
    return ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

bool skipWhitespace(std::string_view& input)
{
    size_t len = 0;
    while (isWhitespace(input[len]))
        len++;
    input.remove_prefix(len);
    return len > 0;
}

bool skipLineBreak(std::string_view& input)
{
    bool skipped = false;
    while (isLineBreak(input[0]))
    {
        input.remove_prefix(1);
        skipped = true;
    }
    return skipped;
}

bool isNameChar(const char ch)
{
    return std::isalnum(ch) || ch == '_';
}


template<typename Callable>
std::string_view parseWhile(std::string_view& input, Callable c)
{
    size_t len = 0;
    while (c(input[len]))
        len++;
    std::string_view res(input.data(), len);
    input.remove_prefix(len);
    return res;
}

std::string_view parseName(std::string_view& input)
{
    return parseWhile(input, isNameChar);
}

bool skipChar(std::string_view& input, char ch)
{
    if (input[0] == ch)
    {
        input.remove_prefix(1);
        return true;
    }
    return false;
}

bool skipArrayStart(std::string_view& input)
{
    return skipChar(input, '[');
}

bool skipArrayEnd(std::string_view& input)
{
    return skipChar(input, ']');
}

int parseArraySpecifier(std::string_view& input)
{
    if (!skipArrayStart(input))
        return -1;
    std::string_view sizeSpec = parseWhile(input, [](char ch) { return std::isdigit(ch); });
    if (sizeSpec.empty())
        return 0; // dynamic array
    skipArrayEnd(input);
    return std::stoi(std::string(sizeSpec));
}

std::string_view parseUntilEndOfQuotation(std::string_view& input)
{
    return parseWhile(input, [](char ch) { return ch != '"'; });
}

std::string_view parseUntilEndOfLine(std::string_view& input)
{
    return parseWhile(input, [](char ch) { return !isLineBreak(ch); });
}

bool skipTypeSeparator(std::string_view& input)
{
    return skipChar(input, ':');
}

bool skipAssignmentOp(std::string_view& input)
{
    return skipChar(input, '=');
}

bool skipQuotation(std::string_view& input)
{
    return skipChar(input, '"');
}

bool skipArrayElementsSeparator(std::string_view& input)
{
    return skipChar(input, ',');
}

bool skipEndOfAssignment(std::string_view& input)
{
    return skipChar(input, ';');
}

bool skipEndOfLine(std::string_view& input)
{
    return skipLineBreak(input) || input.empty();
}

void select_fg_color(uint8_t color)
{
    printf("\x1B[38;5;%dm", color);
}
void exit_color()
{
    printf("\x1B[m");
}

void reportErrorLocation(const char* lineStart, const std::string_view& currentView)
{
    std::string_view line(lineStart);
    line = parseUntilEndOfLine(line);
    printf("  %.*s\n", int(line.size()), line.data());
    printf("  ");
    size_t pos = currentView.data() - lineStart;
    for (size_t i = 0; i < pos; ++i)
        printf(" ");
    printf("^\n");
}

bool skipStartOfTable(std::string_view& input)
{
    return skipChar(input, '{');
}

bool skipEndOfTable(std::string_view& input)
{
    return skipChar(input, '}');
}

std::tuple<std::string_view, std::string_view, int> parseKey(std::string_view& view)
{
    skipWhitespace(view);
    std::string_view name = parseName(view);
    skipWhitespace(view);
    if (skipTypeSeparator(view))
    {
        skipWhitespace(view);
        std::string_view typeName = parseName(view);
        int arraySize = parseArraySpecifier(view);
        skipWhitespace(view);
        return std::make_tuple(name, typeName, arraySize);
    }
    return std::make_tuple(name, std::string_view{}, -1);
}

std::string_view parseValue(std::string_view& view)
{
    skipWhitespace(view);
    skipQuotation(view);
    std::string_view val = parseUntilEndOfQuotation(view);
    skipQuotation(view);
    skipWhitespace(view);
    return val;
}

TypeParser* getTypeParser(std::string_view typeName, const ParserSuite& psuite)
{
    auto itf = psuite.typeParsers.find(std::string(typeName));
    if (itf == psuite.typeParsers.end())
    {
        printf("Warning: don't have parser for type '%.*s'! Skipping.\n", (int)typeName.size(), typeName.data());
        return nullptr;
    }
    return itf->second;
}

void reportError(const char* message, const char* lineStart, const std::string_view& view)
{
    select_fg_color(160);
    printf("Error: ");
    exit_color();
    printf("%s\n", message);
    reportErrorLocation(lineStart, view);
}

// TODO: better error reporting (custom streams, with cerr as default one)
// TODO: comments parsing
// TODO: support unquoted values
// TODO: check for formatting better
// TODO: proper return if encountering an error
// TODO: check for memory leaks
edat::Table parseView(std::string_view& view, const ParserSuite& psuite)
{
    edat::Table res;
    const char* lineStart = view.data();
    while (view.size() > 0)
    {
        skipWhitespace(view);
        if (skipEndOfTable(view)) // We've exhausted that table
            return res;
        if (skipEndOfLine(view))
        {
            // Just an empty string
            lineStart = view.data();
            continue;
        }
        auto [name, typeName, arraySize] = parseKey(view);
        if (!typeName.empty()) // not a table
        {
            if (!skipAssignmentOp(view))
            {
                reportError("no assignment operator '=' after type", lineStart, view);
                return res;
            }
            if (arraySize >= 0)
            {
                skipWhitespace(view);
                if (!skipArrayStart(view))
                    printf("Error: no array start\n");
                std::vector<std::string_view> stringViewArray;
                while (!skipArrayEnd(view))
                {
                    std::string_view val = parseValue(view);
                    stringViewArray.push_back(val);
                    skipArrayElementsSeparator(view); // this is optional actually
                    skipWhitespace(view);
                }
                if (TypeParser* parser = getTypeParser(typeName, psuite))
                    parser->parseArray(name, stringViewArray, res);
                skipWhitespace(view);
            }
            else
            {
                std::string_view val = parseValue(view);
                if (TypeParser* parser = getTypeParser(typeName, psuite))
                    parser->parseValue(name, val, res);
            }
        }
        else
        {
            if (!skipAssignmentOp(view))
            {
                reportError("wrong format for table", lineStart, view);
                return res;
            }
            skipWhitespace(view);
            if (skipEndOfLine(view))
                lineStart = view.data();
            skipWhitespace(view);
            if (!skipStartOfTable(view))
            {
                reportError("wrong format for table", lineStart, view);
                return res;
            }
            edat::Table subTable = parseView(view, psuite);
            res.set<edat::Table>(name, std::move(subTable));
        }
        skipWhitespace(view);
        if (!skipEndOfAssignment(view))
        {
            if (!skipEndOfLine(view))
            {
                reportError("no end of assignment", lineStart, view);
                return res;
            }
            lineStart = view.data();
        }
    }
    return res;
}

edat::Table parseString(const std::string& input, const ParserSuite& psuite)
{
    std::string_view view = input;
    return parseView(view, psuite);
}

edat::Table parseFile(std::filesystem::path path, const ParserSuite& psuite)
{
    size_t fsize = std::filesystem::file_size(path);
    FILE* f = fopen(path.c_str(), "rb");
    std::string fileBuffer; fileBuffer.resize(fsize);
    fread(fileBuffer.data(), 1, fsize, f);
    fclose(f);

    return edat::parseString(fileBuffer, psuite);
}

}

