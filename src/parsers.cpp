#include "parsers.h"

namespace edat
{

static bool isWhitespace(char ch)
{
    // We want to skip only specific stuff!
    return ch == ' ' || ch == '\t';
}

static bool isLineBreak(char ch)
{
    return ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

static bool skipWhitespace(std::string_view& input)
{
    size_t len = 0;
    while (isWhitespace(input[len]))
        len++;
    input.remove_prefix(len);
    return len > 0;
}

static bool skipLineBreak(std::string_view& input)
{
    bool skipped = false;
    while (isLineBreak(input[0]))
    {
        input.remove_prefix(1);
        skipped = true;
    }
    return skipped;
}

static bool isNameChar(const char ch)
{
    return std::isalnum(ch) || ch == '_';
}

template<typename Callable>
static std::string_view parseWhile(std::string_view& input, Callable c)
{
    size_t len = 0;
    while (c(input[len]))
        len++;
    std::string_view res(input.data(), len);
    input.remove_prefix(len);
    return res;
}

static std::string_view parseName(std::string_view& input)
{
    return parseWhile(input, isNameChar);
}

static bool skipChar(std::string_view& input, char ch)
{
    if (input[0] == ch)
    {
        input.remove_prefix(1);
        return true;
    }
    return false;
}

static bool skipArrayStart(std::string_view& input)
{
    return skipChar(input, '[');
}

static bool skipArrayEnd(std::string_view& input)
{
    return skipChar(input, ']');
}

static int parseArraySpecifier(std::string_view& input)
{
    if (!skipArrayStart(input))
        return -1;
    std::string_view sizeSpec = parseWhile(input, [](char ch) { return std::isdigit(ch); });
    if (sizeSpec.empty())
        return 0; // dynamic array
    skipArrayEnd(input);
    return std::stoi(std::string(sizeSpec));
}

static std::string_view parseUntilEndOfQuotation(std::string_view& input)
{
    return parseWhile(input, [](char ch) { return ch != '"'; });
}

static std::string_view parseUntilEndOfLine(std::string_view& input)
{
    return parseWhile(input, [](char ch) { return !isLineBreak(ch); });
}

static bool skipTypeSeparator(std::string_view& input)
{
    return skipChar(input, ':');
}

static bool skipAssignmentOp(std::string_view& input)
{
    return skipChar(input, '=');
}

static bool skipQuotation(std::string_view& input)
{
    return skipChar(input, '"');
}

static bool skipArrayElementsSeparator(std::string_view& input)
{
    return skipChar(input, ',');
}

static bool skipEndOfAssignment(std::string_view& input)
{
    return skipChar(input, ';');
}

static bool skipEndOfLine(std::string_view& input)
{
    return skipLineBreak(input) || input.empty();
}

static void select_fg_color(uint8_t color)
{
    printf("\x1B[38;5;%dm", color);
}
static void exit_color()
{
    printf("\x1B[m");
}

static void reportErrorLocation(const char* lineStart, const std::string_view& currentView)
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

static bool skipStartOfTable(std::string_view& input)
{
    return skipChar(input, '{');
}

static bool skipEndOfTable(std::string_view& input)
{
    return skipChar(input, '}');
}

static std::tuple<std::string_view, std::string_view, int> parseKey(std::string_view& view)
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

static std::string_view parseValue(std::string_view& view)
{
    skipWhitespace(view);
    skipQuotation(view);
    std::string_view val = parseUntilEndOfQuotation(view);
    skipQuotation(view);
    skipWhitespace(view);
    return val;
}

static TypeParser* getTypeParser(std::string_view typeName, const ParserSuite& psuite)
{
    auto itf = psuite.typeParsers.find(std::string(typeName));
    if (itf == psuite.typeParsers.end())
    {
        printf("Warning: don't have parser for type '%.*s'! Skipping.\n", (int)typeName.size(), typeName.data());
        return nullptr;
    }
    return itf->second;
}

static void reportError(const char* message, const char* lineStart, const std::string_view& view)
{
    select_fg_color(160);
    printf("Error: ");
    exit_color();
    printf("%s\n", message);
    reportErrorLocation(lineStart, view);
}

static bool skipCopyOperator(std::string_view& view)
{
    std::string_view tview = view;
    skipWhitespace(tview);
    if (skipChar(tview, '<') && skipChar(tview, '-'))
    {
        skipWhitespace(tview);
        view = tview;
        return true;
    }

    return false;
}

static std::string_view parseCopyExpression(std::string_view& view)
{
    if (!skipCopyOperator(view))
        return std::string_view{};
    return parseName(view);
}

// TODO: better error reporting (custom streams, with cerr as default one)
// TODO: comments parsing
// TODO: support unquoted values
// TODO: check for formatting better
// TODO: proper return if encountering an error
// TODO: check for memory leaks
edat::Table parseView(std::string_view& view, const ParserSuite& psuite, const edat::Table* cloneFrom = nullptr)
{
    edat::Table res;
    if (cloneFrom)
        res = cloneTable(*cloneFrom);
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
            std::string_view copyFrom = parseCopyExpression(view);
            edat::Table subTable;
            if (!copyFrom.empty())
            {
                res.get<Table>(copyFrom, [&](const edat::Table& tbl)
                {
                    subTable = cloneTable(tbl);
                });
                skipWhitespace(view);
            }
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
            edat::Table resSubTable = parseView(view, psuite, &subTable);
            res.set<edat::Table>(name, std::move(resSubTable));
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
