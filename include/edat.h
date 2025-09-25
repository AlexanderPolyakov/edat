#pragma once

// TODO: replace with better containers eventually?
#include <vector>
#include <string_view>
#include <string>
#include <unordered_map>

namespace edat
{

struct ValueStorage
{
    virtual ~ValueStorage() {}
};

// Do we need classes here? Storing ptr to underlying container might be enough?
template<typename T>
struct TypedStorage : public ValueStorage
{
    std::vector<T> storage;

    virtual ~TypedStorage<T>() {} // just do the automatic stuff
};

struct Table
{
    struct TableRecord
    {
        size_t nameId = -1;
        size_t storageId = -1;
        size_t idx = -1;
    };

    // TODO: think about how to remove duplicate names here as we have the same name in `names` and `nameMap` (for quick lookup)
    std::vector<std::string> names;
    std::vector<TableRecord> records;
    std::unordered_map<std::string, size_t> nameMap;

    std::unordered_map<size_t, size_t> typeHashMap; // maps type hash_code to index in storages
    std::vector<ValueStorage*> storages;

    Table() = default;
    Table(Table&& rhs) = default;
    Table& operator=(Table&& rhs) = default;
    // Clean all the storages!
    ~Table()
    {
        for (ValueStorage* storage : storages)
            delete storage;
    }

    // TODO: think about it, maybe standalone inline functions will work best here?
    // Syntax sugar is good, but keeping everything tidy and clean might be better?
    TableRecord findIndex(const std::string_view& name) const
    {
        auto itf = nameMap.find(std::string(name));
        if (itf == nameMap.end())
            return {size_t(-1), size_t(-1)};
        return records[itf->second];
    }

    template<typename T>
    TypedStorage<T>* getTypedStorage(size_t storageId)
    {
        return (TypedStorage<T>*)storages[storageId];
    }

    template<typename T>
    const TypedStorage<T>* getTypedStorage(size_t storageId) const
    {
        return (TypedStorage<T>*)storages[storageId];
    }

    template<typename T>
    size_t getOrCreateStorageForType()
    {
        const size_t typeHash = typeid(T).hash_code();
        auto itf = typeHashMap.find(typeHash);
        if (itf == typeHashMap.end())
        {
            // We don't have that type registered yet, register
            typeHashMap.emplace(typeHash, storages.size());
            storages.emplace_back(new TypedStorage<T>());
        }
        return typeHashMap[typeHash];
    }

    template<typename T>
    size_t getStorageByType() const
    {
        const size_t typeHash = typeid(T).hash_code();
        auto itf = typeHashMap.find(typeHash);
        if (itf == typeHashMap.end())
            return size_t(-1);
        // We can safely access it here now
        return itf->second;
    }

    template<typename T>
    T getOr(const std::string_view& name, T def) const
    {
        const TableRecord rec = findIndex(name);
        if (rec.storageId < storages.size() && rec.storageId == getStorageByType<T>())
            return getTypedStorage<T>(rec.storageId)->storage[rec.idx];
        return def;
    }

    template<typename T, typename Callable>
    void get(const std::string_view& name, Callable c) const
    {
        const TableRecord rec = findIndex(name);
        if (rec.storageId < storages.size() && rec.storageId == getStorageByType<T>())
            c(getTypedStorage<T>(rec.storageId)->storage[rec.idx]);
    }

    template<typename T>
    void set(const std::string_view& name, T&& value)
    {
        const TableRecord rec = findIndex(name);
        if (rec.storageId < storages.size()) // we have this value already, just need to set it
        {
            getTypedStorage<T>(rec.storageId)->storage[rec.idx] = std::move(value);
            return;
        }

        // Otherwise - create the value
        const size_t storageId = getOrCreateStorageForType<T>();
        TypedStorage<T>* tstorage = getTypedStorage<T>(storageId);
        const size_t idx = tstorage->storage.size();

        // Push the value itself
        tstorage->storage.push_back(std::move(value));

        // Update the nameMap with newly pushed value
        size_t nameId = names.size();
        names.emplace_back(name);
        size_t recordIdx = records.size();
        records.emplace_back(TableRecord{nameId, storageId, idx});
        nameMap.emplace(std::string(name), recordIdx);
    }

    template<typename T, typename Callable>
    void getAll(Callable c) const
    {
        const size_t typeHash = typeid(T).hash_code();
        auto itf = typeHashMap.find(typeHash);
        if (itf == typeHashMap.end())
            return;

        const size_t storageId = itf->second;
        const TypedStorage<T>* tstorage = getTypedStorage<T>(storageId);
        for (const TableRecord& record : records)
            if (record.storageId == storageId)
                c(names[record.nameId], tstorage->storage[record.idx]);
    }
};

}

