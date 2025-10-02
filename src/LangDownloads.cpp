#pragma once
#include <Header1.hpp>

#include <Geode/loader/SettingV3.hpp>

#include <Geode/modify/MenuLayer.hpp>
class $modify(MenuLayer) {
    struct Fields {
        EventListener<web::WebTask> m_listener;
    };
    bool init() {
        if (!MenuLayer::init()) return false;

        if (!SETTING(bool, "Check for official lang pack updates")) return true;

        auto repo = getMod()->getMetadataRef().getLinks().getSourceURL().value_or("");
        auto listURL = repo + "/blob/HEAD/!LANG.PACKS.LIST?raw=true"; // longest repo getting ever cool ^
        m_fields->m_listener.bind(
            [=](web::WebTask::Event* e) {
                if (web::WebResponse* res = e->getValue()) {
                    if (res->code() == 200) {
                        auto str = res->string().unwrapOr("");
                        for (auto entryStr : string::split(str, "\n")) {
                            auto entry = string::split(string::replace(entryStr, " ", ""), ";");
                            if (entry.size() != 3) continue;
                            std::string code = entry[0].c_str();
                            if (code != TextTranslations::get()->getID()) continue;
                            std::string download = entry[2].c_str();
                            if (download != getMod()->getSavedValue<std::string>("langSourceDownloadLink", download)) {
                                auto asd = createQuickPopup(
                                    "Language pack was updated!",
                                    "Reinstall " + code + " lang pack in settings.\n(Installed packs in list have delete action instead of download)",
                                    "Later", "Settings",
                                    [&](void*, int a) { a ? [] {  openInfoPopup(Mod::get()); openSettingsPopup(Mod::get()); }() : void(); }
                                    , false
                                );
                                if (Ref a = this) asd->m_scene = a;
                                asd->show();
                            }
                        }
                    }
                }
            }
        );
        m_fields->m_listener.setFilter(web::WebRequest().get(listURL));

        queueInMainThread(
            [=]() {
                if (!fileExistsInSearchPaths(".hi"_spr)) return;
                auto asd = createQuickPopup(
                    "Install language pack?",
                    "Seems you don't have any language pack installed yet.\n"
                    "Or Localizations has been updated.\n"
                    "You can install it now in settings!",
                    "Later", "Settings",
                    [&](void*, int a) { a ? [] { openInfoPopup(Mod::get()); openSettingsPopup(Mod::get()); }() : void(); }
                    , false
                );
                if (Ref a = this) asd->m_scene = a;
				asd->show();
            }
        );

        return true;
    }
};

inline void downloadLanguagePack(std::string const& download) {
	Ref nt = Notification::create("Downloading: i%", NotificationIcon::Loading, 0);
    nt->show();
    static EventListener<web::WebTask> m_listener;
	m_listener.setFilter(web::WebRequest().get(download));
	m_listener.bind(
        [=](web::WebTask::Event* e) {
            if (web::WebResponse* res = e->getValue()) {
                if (res->code() == 200) {
                    auto resDir = getMod()->getResourcesDir().parent_path();
                    fs::remove_all(resDir, fs::err);
                    fs::createDirectory(resDir);
                    res->into(resDir / "temp.zip");
                    fs::Unzip::intoDir(resDir / "temp.zip", resDir, true);
                    if (nt) nt->hide();
                    getMod()->setSavedValue<std::string>("langSourceDownloadLink", download);
                    TextTranslations::get()->setID("");
                    TextTranslations::value().clear();
                    cocos::reloadTextures(
                        []() {
                            queueInMainThread([]() { openInfoPopup(Mod::get()); openSettingsPopup(Mod::get()); });
                            return ModsLayer::get();
                        }
                    );
                }
                else {
                    if (nt) nt->setString(fmt::format("Response code is {}!", res->code()));
                    if (nt) nt->setIcon(NotificationIcon::Error);
                    if (nt) nt->setTime(3.f);
                    if (nt) nt->waitAndHide();
                }
            }
            else if (web::WebProgress* p = e->getProgress()) {
                if (nt) nt->setString(fmt::format("Downloading: {}%", (int)p->downloadProgress().value_or(0.f)));
            }
        }
    );
}

class thelangselector : public SettingBaseValueV3<matjson::Value> {
public:

    static Result<std::shared_ptr<SettingV3>> parse(std::string const& key, std::string const& modID, matjson::Value const& json) {
        auto res = std::make_shared<thelangselector>();
        auto root = checkJson(json, "thelangselector");
        res->parseBaseProperties(key, modID, root);
        root.checkUnknownKeys();
        return root.ok(std::static_pointer_cast<SettingV3>(res));
    }

    class Node : public SettingValueNodeV3<thelangselector> {
    public:
        EventListener<web::WebTask> m_listener;
        void setupList(std::string const& str) {
			if (!this->getParent()) return log::error("parent of {} is {}", this, this->getParent());
            Ref parent = this->getParent();
            for (auto entryStr : string::split(str, "\n")) {
                auto entry = string::split(string::replace(entryStr, " ", ""), ";");
                if (entry.size() != 3) continue;
				std::string code = entry[0].c_str();
				std::string image = entry[1].c_str();
				std::string download = entry[2].c_str();
                auto sprite = LazySprite::create({ parent->getContentWidth(), 32.f }, true);
                sprite->loadFromUrl(image, CCImage::kFmtUnKnown, true);
                sprite->setAutoResize(true);
                Ref item = CCMenuItemExt::createSpriteExtra(
                    sprite, [=](void*) {
                        if (TextTranslations::get()->getID() == code) {
                            MDPopup::create(
                                "Delete language pack",
                                "Are you sure you want to delete installed language pack?",
                                "Cancel", "Delete",
                                [=](bool a) {
                                    if (!a) return;
                                    auto resDir = getSetting()->getMod()->getResourcesDir().parent_path();
                                    fs::remove_all(resDir, fs::err);
                                    fs::createDirectory(resDir);
                                    Notification::create("Installed language resources deleted!");
                                    TextTranslations::get()->setID("");
                                    TextTranslations::value().clear();
                                    cocos::reloadTextures(
                                        []() {
                                            queueInMainThread([]() { openInfoPopup(Mod::get()); openSettingsPopup(Mod::get()); });
                                            return ModsLayer::get();
                                        }
                                    );
                                }
                            )->show();
                        }
                        else {
                            MDPopup::create(
                                "Download language pack",
                                "Are you sure you want to download\nand install `" + code + "` language pack?"
                                "\n" + download,
                                "Cancel", "Install",
                                [=](bool a) { a ? downloadLanguagePack(download) : void(); }
                            )->show();
                        }
                    }
                );
                item->m_colorEnabled = true;
                item->m_animationEnabled = false;
                Ref menu = CCMenu::create();
                menu->addChild(item);
                menu->setID(code);
                menu->setLayout(SimpleRowLayout::create());
                menu->setContentSize(item-> getContentSize());
                item->setPosition(menu->getContentSize() / 2.f);
                parent->addChild(menu, 3);
            }
            parent->updateLayout();
        }
        void customSetup() {
            if (!this->getParent()) return log::error("parent of {} is {}", this, this->getParent());
            Ref parent = this->getParent();
            // loading notify
            Ref notify = Notification::create(
                "Loading language packs list...", NotificationIcon::Loading
            );
            notify->setPosition(this->getContentSize() / 2.f);
            notify->setPositionY(-32.f);
            this->addChild(notify);
            // load
            auto repo = getSetting()->getMod()->getMetadataRef().getLinks().getSourceURL().value_or("");
            auto listURL = repo + "/blob/HEAD/!LANG.PACKS.LIST?raw=true"; // longest repo getting ever cool ^
            m_listener.bind(
                [=, me = Ref(this), nt = notify](web::WebTask::Event* e) {
                    if (web::WebResponse* res = e->getValue()) {
                        if (res->code() == 200) {
                            if (nt) nt->removeMeAndCleanup();
                            if (me) me->setupList(res->string().unwrapOr(""));
                        }
                        else {
                            if (nt) nt->setString(fmt::format("Response code is {}!", res->code()));
                            if (nt) nt->setIcon(NotificationIcon::Error);
                        }
                    }
                }
            );
            m_listener.setFilter(web::WebRequest().get(listURL));
            //delete btn
            Ref deleteBtn = CCMenuItemExt::createSpriteExtraWithFrameName(
                "GJ_completesIcon_001.png", 0.650f, [=](void*) {}
            );
            parent->addChild(deleteBtn);
            deleteBtn->setContentSize(CCSizeZero);
            deleteBtn->runAction(CCRepeatForever::create(CCSequence::create(CallFuncExt::create(
                [=]() { 
                    auto parent = deleteBtn->getParent();
                    if (!parent) return;
                    auto targ = parent->getChildByID(TextTranslations::get()->getID());
                    if (deleteBtn) deleteBtn->setVisible(!TextTranslations::get()->getID().empty());
                    if (deleteBtn) deleteBtn->setPositionY(targ ? targ->getPositionY() : 9999.f);
                    if (deleteBtn) deleteBtn->setPositionX(parent->getContentWidth() - 20.f);
                }
            ), nullptr)));
        }
        bool init(std::shared_ptr<thelangselector> setting, float width) {
            if (!SettingValueNodeV3::init(setting, width)) return false;
            this->setContentHeight(20.000);
            if (auto a = getChildByType<CCLayerColor>(0)) {
                // darker pls :3
                for (auto w : { 40, 60, 80, 100 }) this->addChild(a, -6);
            }
            queueInMainThread([asd = Ref(this)] { asd->customSetup(); });
            return true;
        }
        static Node* create(std::shared_ptr<thelangselector> setting, float width) {
            auto ret = new Node();
            if (ret && ret->init(setting, width)) {
                ret->autorelease();
                return ret;
            }
            CC_SAFE_DELETE(ret);
            return nullptr;
        }
    };

    SettingNodeV3* createNode(float width) override {
        return Node::create(std::static_pointer_cast<thelangselector>(shared_from_this()), width);
    };

};

void nails() {
    auto ret = Mod::get()->registerCustomSettingType("thelangselector", &thelangselector::parse);
    if (!ret) log::error("Unable to register setting type: {}", ret.err());
}
$on_mod(Loaded) { nails(); }