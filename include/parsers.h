#pragma once

#include <functional>
#include <filesystem>
#include "edat.h"

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

edat::Table parseString(const std::string& input, const ParserSuite& psuite);
edat::Table parseFile(std::filesystem::path path, const ParserSuite& psuite);

}

