#pragma once
#include <Header1.hpp>

#include <Geode/loader/SettingV3.hpp>

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
				std::string code = entry[0].c_str();
				std::string image = entry[1].c_str();
				std::string download = entry[2].c_str();
                Ref item = CCMenuItemExt::createSpriteExtra(
                    LazySprite::create({ 32.f, parent->getContentWidth() }, true),
                    [=](void*) {

                    }
                );
                Ref menu = CCMenu::create();
                menu->addChild(item);
                menu->setLayout(SimpleRowLayout::create());
                menu->setContentWidth(parent->getContentWidth());
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
            auto repo = getSetting()->getMod()->getMetadata().getLinks().getSourceURL().value_or("");
            auto listURL = repo + "/blob/HEAD/!LANG.PACKS.LIST"; // longest repo getting ever cool ^
            m_listener.bind(
                [=, me = Ref(this), nt = notify](web::WebTask::Event* e) {
                    if (web::WebResponse* res = e->getValue()) {
                        if (res->code() == 200) {
                            if (nt) nt->hide();
                            if (me) me->setupList(res->string().unwrapOr(""));
                        }
                        else {
                            if (nt) nt->setString(fmt::format("Response code is {}!", res->code()));
                            if (nt) nt->setIcon(NotificationIcon::Error);
                        }
                    }
                    else if (e->isCancelled()) {
                        if (nt) nt->setString("Loading is cancelled.");
                        if (nt) nt->setIcon(NotificationIcon::Error);
                    }
                }
            );
            m_listener.setFilter(web::WebRequest().get(listURL));
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