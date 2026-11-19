#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <chrono>

using namespace geode::prelude;

struct Particula {
    CCSprite* sprite = nullptr;
    float y = 0.f;
    float vy = 0.f;
    float offset = 0.f;
    bool viva = false;
};

struct AtomoEstado {
    bool ativo = false;
    bool dividido = false;
    bool segurando = false;
    std::chrono::steady_clock::time_point ultimoToque{};
    Particula part[2];
} g;

constexpr float ACEL = 1400.f;
constexpr float VEL_MAX = 380.f;
constexpr float DISTANCIA = 55.f;
constexpr float CHAO_Y = 105.f;
constexpr float TETO_Y = 405.f;
constexpr float TAMANHO = 18.f;
constexpr int JANELA_TOQUE_DUPLO_MS = 250;

class $modify(AtomoPlayer, PlayerObject) {
    bool pushButton(PlayerButton btn) {
        if (g.ativo && btn == PlayerButton::Jump) {
            g.segurando = true;
            auto agora = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                agora - g.ultimoToque).count();
            g.ultimoToque = agora;

            if (ms < JANELA_TOQUE_DUPLO_MS) {
                auto pl = PlayLayer::get();
                if (pl && this == pl->m_player1) {
                    float cy = this->getPositionY();
                    g.dividido = !g.dividido;
                    for (int i = 0; i < 2; i++) {
                        auto& p = g.part[i];
                        p.viva = g.dividido;
                        p.vy = 0.f;
                        p.offset = (i == 0 ? DISTANCIA : -DISTANCIA);
                        p.y = cy + p.offset;
                        if (p.sprite) p.sprite->setVisible(g.dividido);
                    }
                }
            }
        }
        return PlayerObject::pushButton(btn);
    }

    bool releaseButton(PlayerButton btn) {
        if (g.ativo && btn == PlayerButton::Jump) g.segurando = false;
        return PlayerObject::releaseButton(btn);
    }
};

class $modify(AtomoPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        g = AtomoEstado{};
        g.ativo = Mod::get()->getSettingValue<bool>("atomo-sempre-ativo");
        if (!g.ativo) return;

        m_player1->toggleFlyMode(true, false);

        for (int i = 0; i < 2; i++) {
            auto s = CCSprite::createWithSpriteFrameName("player_01_001.png");
            if (!s) continue;
            s->setScale(0.5f);
            s->setVisible(false);
            s->setZOrder(100);
            m_objectLayer->addChild(s);
            g.part[i].sprite = s;
        }
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        if (!g.ativo) return;
        g.dividido = false;
        g.segurando = false;
        for (auto& p : g.part) {
            p.viva = false;
            if (p.sprite) p.sprite->setVisible(false);
        }
        m_player1->toggleFlyMode(true, false);
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!g.ativo || !g.dividido) return;

        float x = m_player1->getPositionX();

        for (auto& p : g.part) {
            if (!p.viva || !p.sprite) continue;

            p.vy += (g.segurando ? ACEL : -ACEL) * dt;
            p.vy = std::clamp(p.vy, -VEL_MAX, VEL_MAX);
            p.y += p.vy * dt;

            bool quebrou = (p.y < CHAO_Y || p.y > TETO_Y);

            CCRect r(x - TAMANHO / 2, p.y - TAMANHO / 2, TAMANHO, TAMANHO);
            if (!quebrou) {
                for (auto obj : CCArrayExt<GameObject*>(m_objects)) {
                    if (!obj) continue;
                    if (std::abs(obj->getPositionX() - x) > 60.f) continue;
                    auto tipo = obj->m_objectType;
                    if (tipo != GameObjectType::Hazard &&
                        tipo != GameObjectType::Solid) continue;
                    if (r.intersectsRect(obj->getObjectRect())) {
                        quebrou = true;
                        break;
                    }
                }
            }

            if (quebrou) {
                p.viva = false;
                p.sprite->setVisible(false);
                continue;
            }

            p.sprite->setPosition({x, p.y});
        }
    }
};
