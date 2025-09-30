#pragma once //rate this cool header file name here
#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
using namespace geode::prelude;

#include <regex>

namespace fs {
    static std::error_code err;
    using namespace file;
    using namespace std::filesystem;
};
namespace matjson {
    inline auto valOrValsArr(matjson::Value value) {
        if (value.isArray()) return value;
        return matjson::parse("[" + value.dump() + "]").unwrapOrDefault();
    }
}
#define SETTING(type, key_name) Mod::get()->getSettingValue<type>(key_name)

#include <alphalaneous.alphas_geode_utils/include/Utils.h>

#include <TextTranslations.hpp>