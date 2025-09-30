#pragma once
#include <Header1.hpp>

inline auto getNodeParentsTree(CCNode* node) {
    auto value = matjson::Value();
    if (!node) return value;
    while (node->m_pParent) {
        auto name = AlphaUtils::Cocos::getClassName(node);
        auto id = node->getID();
        auto item = matjson::Value();
        item.push(name);
		item.push(id);
        value.push(item);
        node = node->m_pParent;
    }
    return value;
}

inline void escapeRegexChars(std::string& result, const char* str, size_t len) {
    static const char special[] = ".^$*+?()[]{}|\\";
    for (size_t i = 0; i < len; ++i) {
        if (strchr(special, str[i])) result += '\\';
        result += str[i];
    }
}

struct FastSpec {
    char type;
    uint8_t len;
    inline bool valid() const { return len > 0; }
};
inline FastSpec parseSpec(const char* str, size_t pos, size_t size) {
    if (pos >= size or str[pos] != '%') return { 0, 0 };
    size_t i = pos + 1;
    if (i >= size) return { 0, 0 };
    char c = str[i];
    if (c == '%') return { '%', 2 };
    while (i < size && (isdigit(str[i]) or str[i] == '.' or
        str[i] == 'l' or str[i] == 'h' or str[i] == 'z' or str[i] == 't')) {
        i++;
    }
    if (i < size) {
        c = str[i];
        if (strchr("sdifuoxXeEgGcp", c)) {
            return { c, static_cast<uint8_t>(i - pos + 1) };
        }
    }
    return { 0, 0 };
}

inline const char* getRegex(char type) {
    switch (type) {
    case 's': return "(.+?)";
    case 'd': case 'i': return "([-+]?\\d+)";
    case 'u': case 'o': case 'x': case 'X': return "(\\d+)";
    case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': return "([-+]?\\d*\\.?\\d+(?:[eE][-+]?\\d+)?)";
    case 'c': return "(.)";
    case 'p': return "(0x[0-9a-fA-F]+)";
    default: return "(.+?)";
    }
}

struct FormatPatternMatch {
    std::string regexPattern;
    std::string replacement;
    bool isValid = false;
};

inline std::optional<FormatPatternMatch> convertFormatToRegex(
    const std::string& pat, const std::string& trans
) {
    if (pat.empty() or pat.size() > 256) return std::nullopt;

    FormatPatternMatch res;
    std::string regex;
    std::string repl;
    regex.reserve(pat.size() * 2);
    repl.reserve(trans.size() + 20);

    regex = "^";

    const char* pstr = pat.c_str();
    const char* tstr = trans.c_str();
    size_t plen = pat.size();
    size_t tlen = trans.size();

    size_t ppos = 0;
    size_t tpos = 0;
    int grp = 1;
    uint8_t patCount = 0;

    while (ppos < plen) {
        const char* ph = strchr(pstr + ppos, '%');
        size_t phpos = ph ? (ph - pstr) : std::string::npos;

        if (phpos == std::string::npos) {
            escapeRegexChars(regex, pstr + ppos, plen - ppos);
            break;
        }

        if (phpos > ppos) {
            escapeRegexChars(regex, pstr + ppos, phpos - ppos);
        }

        FastSpec spec = parseSpec(pstr, phpos, plen);
        if (!spec.valid()) return std::nullopt;

        if (spec.type != '%') {
            regex += getRegex(spec.type);
            patCount++;
        }
        else {
            regex += '%';
        }

        ppos = phpos + spec.len;
    }

    regex += '$';

    uint8_t transCount = 0;
    while (tpos < tlen) {
        const char* ph = strchr(tstr + tpos, '%');
        size_t phpos = ph ? (ph - tstr) : std::string::npos;

        if (phpos == std::string::npos) {
            repl.append(tstr + tpos, tlen - tpos);
            break;
        }

        repl.append(tstr + tpos, phpos - tpos);

        FastSpec spec = parseSpec(tstr, phpos, tlen);
        if (!spec.valid()) {
            tpos = phpos + 1;
            repl += '%';
            continue;
        }

        if (spec.type != '%') {
            if (grp <= patCount) {
                repl += '$';
                repl += ('0' + grp);
                grp++;
                transCount++;
            }
            else {
                repl += '%';
                repl += spec.type;
            }
        }
        else {
            repl += '%';
        }

        tpos = phpos + spec.len;
    }

    if (patCount != transCount) {
        log::warn("Placeholder mismatch: {}!={}", patCount, transCount);
    }

    res.regexPattern = std::move(regex);
    res.replacement = std::move(repl);
    res.isValid = true;

    return res;
}

static std::unordered_map<std::string, std::pair<std::regex, std::string>> cache;
static std::unordered_set<std::string> invalid;

void initTranslationCache() {
    static bool initialized;
    if (initialized) return;
    cache.reserve(128);
    invalid.reserve(32);
    initialized = true;
}

std::string tryMatchFormatPattern(const std::string& str) {
    if (str.empty() or str.size() > 512 or strchr(str.c_str(), '%')) return str;

    initTranslationCache();

    for (auto& [key, val] : TextTranslations::value()) {
        if (!strchr(key.c_str(), '%')) continue;
        if (invalid.count(key)) continue;

        std::string trans;
        if (val.isString()) {
            trans = val.asString().unwrapOrDefault();
        }
        else if (val.isObject() && val.contains("text")) {
            trans = val["text"].asString().unwrapOrDefault();
        }

        if (trans.empty()) continue;

        auto it = cache.find(key);
        if (it == cache.end()) {
            auto m = convertFormatToRegex(key, trans);
            if (!m or !m->isValid) {
                invalid.insert(key);
                continue;
            }

            auto [newIt, _] = cache.emplace(key, std::make_pair(
                std::regex(m->regexPattern, std::regex::optimize), std::move(m->replacement)
            ));
            it = newIt;
        }

        std::smatch match;
        if (std::regex_match(str, match, it->second.first)) {
            std::string res = std::regex_replace(str, it->second.first, it->second.second);
            if (res != str) {
                log::debug("Fmt '{}': '{}' -> '{}'", key, str, res);
                return res;
            }
        }
    }

    return str;
}

static std::vector<std::tuple<std::regex, std::string, std::string>> genRules;

void compileGenerationRules() {
    static bool compiled;
    if (compiled) return;

    auto rules = TextTranslations::value()["_generation_rules"];
    if (!rules.isArray()) {
        compiled = true;
        return;
    }

    genRules.reserve(32);

    for (auto& rule : rules) {
        if (!rule.isObject()) continue;

        if (!rule["enabled"].asBool().unwrapOr(true)) continue;

        auto pat = rule["pattern"].asString().unwrapOrDefault();
        auto repl = rule["replacement"].asString().unwrapOrDefault();
        auto mode = rule["mode"].asString().unwrapOr("replace");

        if (pat.empty()) continue;

        genRules.emplace_back(
            std::regex(pat, std::regex::optimize | std::regex::nosubs),
            std::move(repl),
            std::move(mode)
        );
    }

    compiled = true;
    log::info("Compiled {} generation rules", genRules.size());
}

std::string generateTranslation(const std::string& str) {
    if (str.empty() or str.size() > 512) return str;

    auto fmt = tryMatchFormatPattern(str);
    if (fmt != str) return fmt;

    compileGenerationRules();
    if (genRules.empty()) return str;

    for (auto& [regex, repl, mode] : genRules) {
        if (!std::regex_search(str, regex)) continue;

        std::string res;
        if (mode == "append") {
            res = str + repl;
        }
        else if (mode == "prepend") {
            res = repl + str;
        }
        else {
            res = std::regex_replace(str, regex, repl);
        }

        if (res != str) {
            log::debug("Gen: '{}' -> '{}'", str, res);
            return res;
        }
    }

    return str;
}

void precompileTranslationPatterns() {
    initTranslationCache();
    compileGenerationRules();

    size_t ok = 0, fail = 0;

    for (auto& [key, val] : TextTranslations::value()) {
        if (!strchr(key.c_str(), '%') or cache.count(key)) continue;

        std::string trans;
        if (val.isString()) {
            trans = val.asString().unwrapOrDefault();
        }
        else if (val.isObject() && val.contains("text")) {
            trans = val["text"].asString().unwrapOrDefault();
        }

        if (trans.empty()) continue;

        auto m = convertFormatToRegex(key, trans);
        if (!m or !m->isValid) {
            invalid.insert(key);
            fail++;
            continue;
        }

        cache.emplace(key, std::make_pair(
            std::regex(m->regexPattern, std::regex::optimize), std::move(m->replacement)
        ));
        ok++;
    }

    log::info("Precompiled {} patterns ({} fail)", ok, fail);
}

bool shouldUpdateWithTranslation(CCNode* node, const char* str) {
    if (!node or !str) return false;

    if (SETTING(bool, "Disable text translations")) return false;

    auto translation = std::string();

    // try to get translation from key:repl or key:{text:repl}
    if (TextTranslations::value()[str].isString()) {
        translation = TextTranslations::value()[str].asString().unwrapOrDefault();
    }
    else {
        translation = TextTranslations::value()[str]["text"].asString().unwrapOrDefault();
    }

    //translation not found
    if (translation.empty()) {
        translation = str;
        auto generated = generateTranslation(str);
        if (generated != str) {
            translation = generated;
            if (TextTranslations::value()["save_generated_translations"].asBool().unwrapOr(true)) {
                TextTranslations::value()[str] = translation;
                log::debug("Saved generated translation: '{}' -> '{}'", str, translation);
            }
        }
        else return false; // translation not generated
    };

    auto parentsTree = getNodeParentsTree(node).dump();

    if (string::contains(parentsTree, "TooltipNode")) return false;
    if (string::contains(parentsTree, "MultilineBitmapFont")) return false;
    //exceptions
    for (auto needle : matjson::valOrValsArr(TextTranslations::value()[str]["node-tree-exceptions"])) {
        auto asd = needle.asString().unwrapOrDefault();
        if (asd.size() and string::contains(parentsTree, asd)) return false;
    }
    //required nodes
    for (auto needle : matjson::valOrValsArr(TextTranslations::value()[str]["node-tree-required"])) {
        auto asd = needle.asString().unwrapOrDefault();
        if (asd.size() and !string::contains(parentsTree, asd)) return false;
    }

    auto isInpPlaceholder = false;
    if (auto label = typeinfo_cast<CCLabelBMFont*>(node)) {
        auto col = label->getColor();
        isInpPlaceholder = ccc3BEqual(col, { 120,170,240 }) ? true : isInpPlaceholder;
        isInpPlaceholder = ccc3BEqual(col, { 108,153,216 }) ? true : isInpPlaceholder;
        isInpPlaceholder = ccc3BEqual(col, { 150,150,150 }) ? true : isInpPlaceholder;
    }
    if (string::contains(parentsTree, "CCTextInputNode") and !isInpPlaceholder) return false;

    std::vector<std::string> wIDS = {
        "creator-name",
        "level-name",
        "username-button",
        "song-name-label",
        "artist-label",
    };
    for (auto needle : matjson::valOrValsArr(TextTranslations::value()[str]["node-id-exceptions"])) {
        for (auto item : string::split(needle.asString().unwrapOrDefault(), ",")) wIDS.push_back(item);
    }

    std::string nodeID = node->getID();
    if (string::containsAny(nodeID, wIDS)) return false;

    if (auto parent = node->getParent()) {
        std::string parentID = parent->getID();
        if (string::containsAny(parentID, wIDS)) return false;
    }

    return true;
}

#include "Geode/modify/CCLabelBMFont.hpp"
class $modify(GDL_CCLabelBMFont, CCLabelBMFont) {
    bool tryUpdateWithTranslation(char const* str) {
        if (!str or !shouldUpdateWithTranslation(this, str)) return false;

        if (this->getString() != std::string(str)) return false;

        auto translation = TextTranslations::value()[str].asString().unwrapOrDefault();
        if (translation.empty()) return false;

        auto point = typeinfo_cast<CCNode*>(this->getUserObject("translation-point"_spr));
        if (point) {
            if (translation != point->getID()) {
                this->setContentSize(point->getContentSize());
                this->setScale(point->getScale());
                this->setUserObject("translation-point"_spr, nullptr);
                return tryUpdateWithTranslation(str);
            }
            else {
                this->setString(translation.c_str());
            }
        }
        else {
            auto size = this->getScaledContentSize();
            auto scale = this->getScale();

            this->setString(translation.c_str());
            limitNodeSize(this, size, this->getScale(), 0.3f);

            point = CCNode::create();
            if (point) { //кековщина
                point->setContentSize(size);
                point->setScale(scale);
                point->setID(translation);
                this->setUserObject("translation-point"_spr, point);
            }
        }
        return true;
    }

    bool initWithString(const char* pszstr, const char* font, float a3, cocos2d::CCTextAlignment a4, cocos2d::CCPoint a5) {
        if (!CCLabelBMFont::initWithString(pszstr, font, a3, a4, a5)) return false;
        // отложенная
        if (pszstr) {
            std::string str = pszstr; // паранорман
            queueInMainThread([__this = Ref(this), str] {
                if (__this and !str.empty()) {
                    __this->tryUpdateWithTranslation(str.c_str());
                }
                });
        }
        return true;
    }

    void setString(const char* newString) {
        CCLabelBMFont::setString(newString ? newString : "");
        this->tryUpdateWithTranslation(newString);
    }
};

#if !defined(GEODE_IS_IOS)

#include "Geode/modify/MultilineBitmapFont.hpp"
class $modify(GDL_MultilineBitmapFont, MultilineBitmapFont) {
    struct Fields {
        float m_textScale = 1.0f;
        std::string m_fontName;
        float m_maxWidth = 0.0f;
    };

    static uint32_t nextUTF8(std::string::iterator & it, std::string::iterator end) {
        if (it >= end)
            return 0;

        unsigned char c1 = static_cast<unsigned char>(*it++);
        if (c1 < 0x80)
            return c1;

        if ((c1 & 0xE0) == 0xC0 and it < end) {
            unsigned char c2 = static_cast<unsigned char>(*it++);
            return ((c1 & 0x1F) << 6) | (c2 & 0x3F);
        }
        if ((c1 & 0xF0) == 0xE0 and it + 1 < end) {
            unsigned char c2 = static_cast<unsigned char>(*it++);
            unsigned char c3 = static_cast<unsigned char>(*it++);
            return ((c1 & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
        }
        if ((c1 & 0xF8) == 0xF0 and it + 2 < end) {
            unsigned char c2 = static_cast<unsigned char>(*it++);
            unsigned char c3 = static_cast<unsigned char>(*it++);
            unsigned char c4 = static_cast<unsigned char>(*it++);
            return ((c1 & 0x07) << 18) | ((c2 & 0x3F) << 12) | ((c3 & 0x3F) << 6) | (c4 & 0x3F);
        }
        return c1;
    }
    static void appendUTF8(uint32_t cp, std::string & str) {
        if (cp < 0x80) {
            str += static_cast<char>(cp);
        }
        else if (cp < 0x800) {
            str += static_cast<char>(0xC0 | (cp >> 6));
            str += static_cast<char>(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000) {
            str += static_cast<char>(0xE0 | (cp >> 12));
            str += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            str += static_cast<char>(0x80 | (cp & 0x3F));
        }
        else {
            str += static_cast<char>(0xF0 | (cp >> 18));
            str += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            str += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            str += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    gd::string readColorInfo(gd::string s) {
        std::string str = s;
        std::string str2;

        for (auto it = str.begin(); it != str.end();) {
            auto cp = nextUTF8(it, str.end());
            str2 += (cp < 128) ? static_cast<char>(cp) : 'W';
        }

        return MultilineBitmapFont::readColorInfo(str2);
    }
    bool initWithFont(const char* p0, gd::string p1, float p2, float p3, cocos2d::CCPoint p4, int p5, bool colorsDisabled) {
        if (!p0) return false;

        m_fields->m_textScale = p2;
        m_fields->m_fontName = p0;
        m_fields->m_maxWidth = p3;

        std::string translatedText = p1;
        if (shouldUpdateWithTranslation(this, p1.c_str())) {
            translatedText = TextTranslations::value()[p1.c_str()].asString().unwrapOr(p1);
        }

        std::string notags = translatedText;
        std::regex tagRegex("(<c.>)|(<\\/c>)|(<d...>)|(<s...>)|(<\\/s>)|(<i...>)|(<\\/i>)");
        notags = std::regex_replace(translatedText, tagRegex, "");

        if (!MultilineBitmapFont::initWithFont(p0, notags, p2, p3, p4, p5, true))
            return false;

        if (!colorsDisabled) {
            m_specialDescriptors = CCArray::create();
            if (!m_specialDescriptors)
                return false;

            m_specialDescriptors->retain();

            MultilineBitmapFont::readColorInfo(translatedText);

            if (m_specialDescriptors and m_characters) {
                for (auto i = 0u; i < m_specialDescriptors->count(); i++) {
                    auto tag = static_cast<TextStyleSection*>(m_specialDescriptors->objectAtIndex(i));
                    if (!tag) continue;

                    if (tag->m_endIndex == -1 and tag->m_styleType == TextStyleType::Delayed) {
                        if (tag->m_startIndex >= 0 and tag->m_startIndex < static_cast<int>(m_characters->count())) {
                            auto child = static_cast<CCFontSprite*>(m_characters->objectAtIndex(tag->m_startIndex));
                            if (child) {
                                child->m_fDelay = tag->m_delay;
                            }
                        }
                    }
                    else {
                        int startIndex = std::max(0, tag->m_startIndex);
                        int endIndex = std::min(tag->m_endIndex, static_cast<int>(m_characters->count() - 1));

                        for (int j = startIndex; j <= endIndex and j >= 0; j++) {
                            if (j < static_cast<int>(m_characters->count())) {
                                auto child = static_cast<CCFontSprite*>(m_characters->objectAtIndex(j));
                                if (!child) continue;

                                switch (tag->m_styleType) {
                                case TextStyleType::Colored: {
                                    child->setColor(tag->m_color);
                                } break;
                                case TextStyleType::Instant: {
                                    child->m_bUseInstant = true;
                                    child->m_fInstantTime = tag->m_instantTime;
                                } break;
                                case TextStyleType::Shake: {
                                    child->m_nShakeIndex = j;
                                    child->m_fShakeIntensity = static_cast<float>(tag->m_shakeIntensity);
                                    child->m_fShakeElapsed = tag->m_shakesPerSecond <= 0 ? 0.0f : 1.0f / tag->m_shakesPerSecond;
                                } break;
                                default:
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            if (m_specialDescriptors) {
                m_specialDescriptors->release();
                m_specialDescriptors = nullptr;
            }
        }

        return true;
    }
    gd::string stringWithMaxWidth(gd::string p0, float scale, float scaledW) {
        auto width = m_fields->m_maxWidth;

        std::string translatedText = p0;
        if (shouldUpdateWithTranslation(this, p0.c_str())) {
            translatedText = TextTranslations::value()[p0.c_str()].asString().unwrapOr(p0);
        }

        std::string str = translatedText;
        if (auto pos = str.find('\n'); pos != std::string::npos) {
            str = str.substr(0, pos);
        }

        auto lbl = CCLabelBMFont::create("", m_fields->m_fontName.c_str());
        if (!lbl) return p0;

        lbl->setScale(m_fields->m_textScale);

        bool overflown = false;
        std::string current;
        for (auto it = str.begin(); it < str.end();) {
            auto cp = nextUTF8(it, str.end());
            appendUTF8(cp, current);

            lbl->setString(current.c_str());
            if (lbl->getScaledContentSize().width > width) {
                overflown = true;
                break;
            }
        }

        if (overflown) {
            if (auto pos = current.rfind(' '); pos != std::string::npos) {
                current.erase(current.begin() + pos, current.end());
            }
        }
        else {
            current += " ";
        }

        return current;
    }
};

#endif

#include <Geode/modify/CCIMEDispatcher.hpp>
class $modify(GDL_CCIMEDispatcher, CCIMEDispatcher) {

    // почти нахуй не нужный фикс того как винда даёт буковки 
    // (чтоб работало в инпутах гд нужен обход фильтров?)

    void dispatchInsertText(const char* text, int len, enumKeyCodes keys) {
#ifdef GEODE_IS_WINDOWS
        // только вызовы с длиной 1 и факт длиной 2 байта
        if (len == 1 and SETTING(bool, "Fix IME Insert Text on Windows")) {
            size_t actual_len = std::strlen(text);
            if (actual_len == 2) {
                // wchar_t из двух байтов
                wchar_t wch = (static_cast<unsigned char>(text[1]) << 8) | static_cast<unsigned char>(text[0]);

                // в utf8
                char utf8_buffer[8] = { 0 };
                int utf8_len = WideCharToMultiByte(CP_UTF8, 0, &wch, 1, utf8_buffer, sizeof(utf8_buffer), nullptr, nullptr);

                if (utf8_len > 0) {
                    // хых
                    return CCIMEDispatcher::dispatchInsertText(utf8_buffer, utf8_len, keys);
                }
            }
        }
#endif
        return CCIMEDispatcher::dispatchInsertText(text, len, keys);
    }
};