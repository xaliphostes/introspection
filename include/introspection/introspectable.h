#pragma once
#include <introspection/info.h>
#include <introspection/types.h>

/**
 * Base class for introspectable objects.
 */
class Introspectable
{
public:
    virtual ~Introspectable() = default;
    virtual const TypeInfo &getTypeInfo() const = 0;

    // Introspection utility methods
    std::any getMemberValue(const std::string &member_name) const;
    void setMemberValue(const std::string &member_name, const std::any &value);
    std::any callMethod(const std::string &method_name, const std::vector<std::any> &args = {});
    std::vector<std::string> getMemberNames() const;
    std::vector<std::string> getMethodNames() const;
    std::string getClassName() const;
    bool hasMember(const std::string &name) const;
    bool hasMethod(const std::string &name) const;

    void printMemberValue(const std::string &member_name) const;
    void printClassInfo() const;
};

// Simplified macro for introspectable classes
#define INTROSPECTABLE(ClassName)                                    \
public:                                                              \
    static TypeInfo &getStaticTypeInfo()                             \
    {                                                                \
        static TypeInfo info(#ClassName);                            \
        static bool initialized = false;                             \
        if (!initialized)                                            \
        {                                                            \
            registerIntrospection(TypeRegistrar<ClassName>(info));   \
            initialized = true;                                      \
        }                                                            \
        return info;                                                 \
    }                                                                \
    const TypeInfo &getTypeInfo() const override                     \
    {                                                                \
        return getStaticTypeInfo();                                  \
    }                                                                \
                                                                     \
private:                                                             \
    static void registerIntrospection(TypeRegistrar<ClassName> reg); \
                                                                     \
public:

#include "inline/introspectable.hxx"
