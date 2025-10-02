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

class ModsLayer : public CCLayer, public SetIDPopupDelegate {
public:
    class ModListSource;
    class ModList;
    class ModsStatusNode;
    struct UpdateModListStateEvent : public Event {};
    class UpdateModListStateFilter : public EventFilter<UpdateModListStateEvent> {};
    enum class ModListDisplay {
        SmallList,
        BigList,
        Grid,
    };
    CCNode* m_frame;
    std::vector<CCMenuItemSpriteExtra*> m_tabs;
    ModListSource* m_currentSource = nullptr;
    std::unordered_map<ModListSource*, Ref<ModList>> m_lists;
    CCMenu* m_pageMenu;
    CCLabelBMFont* m_pageLabel;
    CCMenuItemSpriteExtra* m_goToPageBtn;
    ModsStatusNode* m_statusNode;
    EventListener<UpdateModListStateFilter> m_updateStateListener;
    bool m_showSearch = true;
    std::vector<CCMenuItemSpriteExtra*> m_displayBtns;
    ModListDisplay m_modListDisplay;
    static ModsLayer* get() {
        //https://github.com/hiimjasmine00/GeodeInPauseMenu/blob/f3462d62204bf7e0345dd1473c36f9d8f91ff190/src/GeodeInPauseMenu.cpp#L7
        auto director = CCDirector::get();
        Ref runningScene = director->getRunningScene();
        auto dontCallWillSwitch = director->getDontCallWillSwitch();
        director->setDontCallWillSwitch(true);

        openModsList();
        Ref<CCLayer> modsLayer;
        if (auto transitionScene = typeinfo_cast<CCTransitionScene*>(director->m_pNextScene)) {
            if (transitionScene->m_pInScene) modsLayer = getChild<CCLayer>(transitionScene->m_pInScene, 0);
        }

        director->setDontCallWillSwitch(true);
        director->replaceScene(runningScene);
        director->setDontCallWillSwitch(dontCallWillSwitch);
        director->m_pNextScene = nullptr;

        if (!modsLayer) return nullptr;
        modsLayer->removeFromParentAndCleanup(false);
        return typeinfo_cast<ModsLayer*>(modsLayer.data());
    }
};