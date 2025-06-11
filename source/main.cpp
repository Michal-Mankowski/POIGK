#include <vector>
#include <algorithm>
#include <functional> 
#include <memory>
#include <cstdlib>
#include <cmath>
#include <ctime>

#include <raylib.h>
#include <raymath.h>

// --- UTILS ---
namespace Utils {
	inline static float RandomFloat(float min, float max) {
		return min + static_cast<float>(rand()) / RAND_MAX * (max - min);
	}
}

// --- TRANSFORM, PHYSICS, LIFETIME, RENDERABLE ---
struct TransformA {
	Vector2 position{};
	float rotation{};
};

struct Physics {
	Vector2 velocity{};
	float rotationSpeed{};
};

struct Renderable {
	enum Size { SMALL = 1, MEDIUM = 2, LARGE = 4 } size = SMALL;
};

// --- RENDERER ---
class Renderer {
public:
	static Renderer& Instance() {
		static Renderer inst;
		return inst;
	}

	void Init(int w, int h, const char* title) {
		InitWindow(w, h, title);
		SetTargetFPS(60);
		screenW = w;
		screenH = h;
	}

	void Begin() {
		BeginDrawing();
		ClearBackground(BLACK);
	}

	void End() {
		EndDrawing();
	}

	void DrawPoly(const Vector2& pos, int sides, float radius, float rot) {
		DrawPolyLines(pos, sides, radius, rot, RED);
	}

	int Width() const {
		return screenW;
	}

	int Height() const {
		return screenH;
	}

private:
	Renderer() = default;

	int screenW{};
	int screenH{};
};

// --- ASTEROID HIERARCHY ---

class Asteroid {
public:
	Asteroid(int screenW, int screenH) {
		init(screenW, screenH);
	}
	virtual ~Asteroid() = default;

	bool Update(float dt) {
		transform.position = Vector2Add(transform.position, Vector2Scale(physics.velocity, dt));
		transform.rotation += physics.rotationSpeed * dt;
		if (transform.position.x < -GetRadius() || transform.position.x > Renderer::Instance().Width() + GetRadius() ||
			transform.position.y < -GetRadius() || transform.position.y > Renderer::Instance().Height() + GetRadius())
			return false;
		return true;
	}
	virtual void Draw() const = 0;

	Vector2 GetPosition() const {
		return transform.position;
	}

	float constexpr GetRadius() const {
		return 16.f * (float)render.size;
	}

	int GetDamage() const {
		return baseDamage * static_cast<int>(render.size);
	}

	int GetSize() const {
		return static_cast<int>(render.size);
	}

protected:
	void init(int screenW, int screenH) {

		render.size = static_cast<Renderable::Size>(1 << GetRandomValue(0, 2));

		switch (GetRandomValue(0, 3)) {
		case 0:
			transform.position = { Utils::RandomFloat(0, screenW), -GetRadius() };
			break;
		case 1:
			transform.position = { screenW + GetRadius(), Utils::RandomFloat(0, screenH) };
			break;
		case 2:
			transform.position = { Utils::RandomFloat(0, screenW), screenH + GetRadius() };
			break;
		default:
			transform.position = { -GetRadius(), Utils::RandomFloat(0, screenH) };
			break;
		}

		float maxOff = fminf(screenW, screenH) * 0.1f;
		float ang = Utils::RandomFloat(0, 2 * PI);
		float rad = Utils::RandomFloat(0, maxOff);
		Vector2 center = {
										 screenW * 0.5f + cosf(ang) * rad,
										 screenH * 0.5f + sinf(ang) * rad
		};

		Vector2 dir = Vector2Normalize(Vector2Subtract(center, transform.position));
		physics.velocity = Vector2Scale(dir, Utils::RandomFloat(SPEED_MIN, SPEED_MAX));
		physics.rotationSpeed = Utils::RandomFloat(ROT_MIN, ROT_MAX);

		transform.rotation = Utils::RandomFloat(0, 360);
	}

	TransformA transform;
	Physics    physics;
	Renderable render;

	int baseDamage = 0;
	static constexpr float LIFE = 10.f;
	static constexpr float SPEED_MIN = 125.f;
	static constexpr float SPEED_MAX = 250.f;
	static constexpr float ROT_MIN = 50.f;
	static constexpr float ROT_MAX = 240.f;
};

class TriangleAsteroid : public Asteroid {
public:
	TriangleAsteroid(int w, int h) : Asteroid(w, h) { baseDamage = 5; }
	void Draw() const override {
		Renderer::Instance().DrawPoly(transform.position, 3, GetRadius(), transform.rotation);
	}
};
class SquareAsteroid : public Asteroid {
public:
	SquareAsteroid(int w, int h) : Asteroid(w, h) { baseDamage = 10; }
	void Draw() const override {
		Renderer::Instance().DrawPoly(transform.position, 4, GetRadius(), transform.rotation);
	}
};
class PentagonAsteroid : public Asteroid {
public:
	PentagonAsteroid(int w, int h) : Asteroid(w, h) { baseDamage = 15; }
	void Draw() const override {
		Renderer::Instance().DrawPoly(transform.position, 5, GetRadius(), transform.rotation);
	}
};


enum class AsteroidShape { TRIANGLE = 3, SQUARE = 4, PENTAGON = 5, RANDOM = 0 };


static inline std::unique_ptr<Asteroid> MakeAsteroid(int w, int h, AsteroidShape shape) {
	switch (shape) {
	case AsteroidShape::TRIANGLE:
		return std::make_unique<TriangleAsteroid>(w, h);
	case AsteroidShape::SQUARE:
		return std::make_unique<SquareAsteroid>(w, h);
	case AsteroidShape::PENTAGON:
		return std::make_unique<PentagonAsteroid>(w, h);
	default: {
		return MakeAsteroid(w, h, static_cast<AsteroidShape>(3 + GetRandomValue(0, 2)));
	}
	}
}

// --- PROJECTILE HIERARCHY ---
enum class WeaponType { LASER, BULLET, COUNT };
class Projectile {
public:
	Projectile(Vector2 pos, Vector2 vel, int dmg, WeaponType wt)
	{
		transform.position = pos;
		physics.velocity = vel;
		baseDamage = dmg;
		type = wt;
	}
	bool Update(float dt) {
		transform.position = Vector2Add(transform.position, Vector2Scale(physics.velocity, dt));

		if (transform.position.x < 0 ||
			transform.position.x > Renderer::Instance().Width() ||
			transform.position.y < 0 ||
			transform.position.y > Renderer::Instance().Height())
		{
			return true;
		}
		return false;
	}
	void Draw() const {
		if (type == WeaponType::BULLET) {
			DrawCircleV(transform.position, 5.f, WHITE);
		}
		else {
			static constexpr float LASER_LENGTH = 30.f;
			Rectangle lr = { transform.position.x - 2.f, transform.position.y - LASER_LENGTH, 4.f, LASER_LENGTH };
			DrawRectangleRec(lr, RED);
		}
	}
	Vector2 GetPosition() const {
		return transform.position;
	}

	float GetRadius() const {
		return (type == WeaponType::BULLET) ? 5.f : 2.f;
	}

	int GetDamage() const {
		return baseDamage;
	}

private:
	TransformA transform;
	Physics    physics;
	int        baseDamage;
	WeaponType type;
};

inline static Projectile MakeProjectile(WeaponType wt, const Vector2 pos, float speed, float rotationDeg)
{
    float rotationRad = DEG2RAD * rotationDeg;
    Vector2 dir = { sinf(rotationRad), -cosf(rotationRad) };
    Vector2 vel = Vector2Scale(dir, speed);
	if (wt == WeaponType::LASER) {
		return Projectile(pos, vel, 20, wt);
	}
	else {
		return Projectile(pos, vel, 10, wt);
	}
}

// --- SHIP HIERARCHY ---
class Ship {
public:
    Ship(int screenW, int screenH)
        : parent(nullptr), orbitRadius(0), orbitAngle(0), score(0)
    {
        transform.position = {
            screenW * 0.5f,
            screenH * 0.5f
        };
        hp = 100;
        speed = 250.f;
        alive = true;

        fireRateLaser = 18.f;
        fireRateBullet = 22.f;
        spacingLaser = 40.f;
        spacingBullet = 20.f;
    }
    virtual ~Ship() = default;
    virtual void Update(float dt) = 0;
    virtual void Draw() const = 0;

    void TakeDamage(int dmg) {
        if (!alive) return;
        hp -= dmg;
        if (hp <= 0) alive = false;
    }

    bool IsAlive() const {
        return alive;
    }

    Vector2 GetPosition() const {
        return transform.position;
    }

    virtual float GetRadius() const = 0;

	virtual float GetRotation() const = 0;

    int GetHP() const {
        return hp;
    }

    float GetFireRate(WeaponType wt) const {
        return (wt == WeaponType::LASER) ? fireRateLaser : fireRateBullet;
    }

    float GetSpacing(WeaponType wt) const {
        return (wt == WeaponType::LASER) ? spacingLaser : spacingBullet;
    }

    int GetScore() const { return score; }
    void AddScore(int s) { score += s; }

    void SetParent(Ship* p, float radius, float angle) {
        parent = p;
        orbitRadius = radius;
        orbitAngle = angle;
    }

    void UpdateOrbit(float dt) {
        if (parent) {
            orbitAngle += dt * 1.0f;
            Vector2 parentPos = parent->GetPosition();
            transform.position = {
                parentPos.x + orbitRadius * cosf(orbitAngle),
                parentPos.y + orbitRadius * sinf(orbitAngle)
            };
        }
    }


    void Ship::UpdateOrbiters(float dt) {
		for (auto& orb : orbiters) {
			orb->UpdateOrbit(dt);
			orb->Update(dt);
			orb->UpdateOrbiters(dt);
		}
		orbiters.erase(
			std::remove_if(orbiters.begin(), orbiters.end(),
				[](const std::unique_ptr<Ship>& orb) { return !orb->IsAlive(); }),
			orbiters.end()
    );
}
    void DrawOrbiters() const {
        for (const auto& orb : orbiters) {
            orb->Draw();
            orb->DrawOrbiters();
        }
    }

    void ShootAll(std::vector<Projectile>& projectiles, WeaponType currentWeapon, float& shotTimer, float dt) {
        if (!IsAlive()) return;
        shotTimer += dt;
        float interval = 1.f / GetFireRate(currentWeapon);
        float projSpeed = GetSpacing(currentWeapon) * GetFireRate(currentWeapon);

        while (shotTimer >= interval) {
            Vector2 shipPos = GetPosition();
            float rotDeg = GetRotation();
            float rotRad = DEG2RAD * rotDeg;
            Vector2 localOffset = { 0.f, -GetRadius() };
            Vector2 rotatedOffset = {
                localOffset.x * cosf(rotRad) - localOffset.y * sinf(rotRad),
                localOffset.x * sinf(rotRad) + localOffset.y * cosf(rotRad)
            };
            Vector2 p = Vector2Add(shipPos, rotatedOffset);
            projectiles.push_back(MakeProjectile(currentWeapon, p, projSpeed, GetRotation()));
            shotTimer -= interval;
        }
        for (auto& orb : orbiters) {
            orb->ShootAll(projectiles, currentWeapon, shotTimer, dt);
        }
    }


    void GetAllShips(std::vector<Ship*>& out) {
        out.push_back(this);
        for (auto& orb : orbiters) {
            orb->GetAllShips(out);
        }
    }

    void TrySpawnOrbiter(int screenW, int screenH, int scoreThreshold, Texture2D* sharedTexture = nullptr);

protected:
    TransformA transform;
    int        hp;
    float      speed;
    bool       alive;
    float      fireRateLaser;
    float      fireRateBullet;
    float      spacingLaser;
    float      spacingBullet;


    Ship* parent;
    float orbitRadius;
    float orbitAngle;
    std::vector<std::unique_ptr<Ship>> orbiters;

    int score;
};
-
class PlayerShip : public Ship {
public:
    PlayerShip(int w, int h, Texture2D* sharedTex = nullptr, float scale_ = 0.3f) : Ship(w, h), scale(scale_) {
        if (sharedTex) {
            texture = *sharedTex;
            ownsTexture = false;
        } else {
            texture = LoadTexture("spaceship1.png");
            GenTextureMipmaps(&texture);
            SetTextureFilter(texture, 2);
            ownsTexture = true;
        }
    }
    ~PlayerShip() {
        if (ownsTexture) UnloadTexture(texture);
    }

    void Update(float dt) override {
        if (!parent) {
            if (alive) {
                if (IsKeyDown(KEY_W)) transform.position.y -= speed * dt;
                if (IsKeyDown(KEY_S)) transform.position.y += speed * dt;
                if (IsKeyDown(KEY_A)) transform.position.x -= speed * dt;
                if (IsKeyDown(KEY_D)) transform.position.x += speed * dt;
                if (IsKeyDown(KEY_Q)) rotation -= 180.0f * dt;
                if (IsKeyDown(KEY_E)) rotation += 180.0f * dt;
            }
            else {
                transform.position.y += speed * dt;
            }
        }

    }

    void Draw() const override {
        if (!alive && fmodf(GetTime(), 0.4f) > 0.2f) return;
        Vector2 center = {
            (texture.width * scale) * 0.5f,
            (texture.height * scale) * 0.5f
        };
        DrawTexturePro(
            texture,
            Rectangle{ 0, 0, (float)texture.width, (float)texture.height },
            Rectangle{ transform.position.x, transform.position.y, texture.width * scale, texture.height * scale },
            center,
            rotation,
            WHITE
        );
    }

    float GetRadius() const override {
        return (texture.width * scale) * 0.5f;
    }

    float GetRotation() const {
        return rotation;
    }

private:
    Texture2D texture;
    float     scale;
    float rotation = 0.0f;
    bool ownsTexture = true;
};
void Ship::TrySpawnOrbiter(int screenW, int screenH, int scoreThreshold, Texture2D* sharedTexture) {
    if (score >= scoreThreshold && orbiters.empty()) {
        auto orb = std::make_unique<PlayerShip>(screenW, screenH, sharedTexture, 0.18f);
        orb->SetParent(this, GetRadius() + 60.f, 0.f);
        orbiters.push_back(std::move(orb));
    }
    for (auto& orb : orbiters) {
        orb->TrySpawnOrbiter(screenW, screenH, scoreThreshold, sharedTexture);
    }
}

// --- APPLICATION ---
class Application {
public:
    static Application& Instance() {
        static Application inst;
        return inst;
    }

    void Run() {
        srand(static_cast<unsigned>(time(nullptr)));
        Renderer::Instance().Init(C_WIDTH, C_HEIGHT, "Asteroids OOP");

        Texture2D sharedTex = LoadTexture("spaceship1.png");
        GenTextureMipmaps(&sharedTex);
        SetTextureFilter(sharedTex, 2);

        auto player = std::make_unique<PlayerShip>(C_WIDTH, C_HEIGHT, &sharedTex);

        float spawnTimer = 0.f;
        float spawnInterval = Utils::RandomFloat(C_SPAWN_MIN, C_SPAWN_MAX);
        WeaponType currentWeapon = WeaponType::LASER;
        float shotTimer = 0.f;

        const int SCORE_THRESHOLD = 3;

        while (!WindowShouldClose()) {
            float dt = GetFrameTime();
            spawnTimer += dt;


            player->Update(dt);
            player->UpdateOrbiters(dt);

            if (!player->IsAlive() && IsKeyPressed(KEY_R)) {
                player = std::make_unique<PlayerShip>(C_WIDTH, C_HEIGHT, &sharedTex);
                asteroids.clear();
                projectiles.clear();
                spawnTimer = 0.f;
                spawnInterval = Utils::RandomFloat(C_SPAWN_MIN, C_SPAWN_MAX);
            }

            if (IsKeyPressed(KEY_ONE)) {
                currentShape = AsteroidShape::TRIANGLE;
            }
            if (IsKeyPressed(KEY_TWO)) {
                currentShape = AsteroidShape::SQUARE;
            }
            if (IsKeyPressed(KEY_THREE)) {
                currentShape = AsteroidShape::PENTAGON;
            }
            if (IsKeyPressed(KEY_FOUR)) {
                currentShape = AsteroidShape::RANDOM;
            }


            if (IsKeyPressed(KEY_TAB)) {
                currentWeapon = static_cast<WeaponType>((static_cast<int>(currentWeapon) + 1) % static_cast<int>(WeaponType::COUNT));
            }


            if (player->IsAlive() && IsKeyDown(KEY_SPACE)) {
                player->ShootAll(projectiles, currentWeapon, shotTimer, dt);
            }
            else {
                float maxInterval = 1.f / player->GetFireRate(currentWeapon);
                if (shotTimer > maxInterval) {
                    shotTimer = fmodf(shotTimer, maxInterval);
                }
            }


            if (spawnTimer >= spawnInterval && asteroids.size() < MAX_AST) {
                asteroids.push_back(MakeAsteroid(C_WIDTH, C_HEIGHT, currentShape));
                spawnTimer = 0.f;
                spawnInterval = Utils::RandomFloat(C_SPAWN_MIN, C_SPAWN_MAX);
            }


            {
                auto projectile_to_remove = std::remove_if(projectiles.begin(), projectiles.end(),
                    [dt](auto& projectile) {
                        return projectile.Update(dt);
                    });
                projectiles.erase(projectile_to_remove, projectiles.end());
            }


            std::vector<Ship*> allShips;
            player->GetAllShips(allShips);

            for (auto pit = projectiles.begin(); pit != projectiles.end();) {
                bool removed = false;

                for (auto ait = asteroids.begin(); ait != asteroids.end(); ++ait) {
                    float dist = Vector2Distance((*pit).GetPosition(), (*ait)->GetPosition());
                    if (dist < (*pit).GetRadius() + (*ait)->GetRadius()) {
                        Ship* closest = nullptr;
                        float minDist = 1e9f;
                        for (auto* s : allShips) {
                            float d = Vector2Distance(s->GetPosition(), pit->GetPosition());
                            if (d < minDist) { minDist = d; closest = s; }
                        }
                        if (closest) closest->AddScore(1);

                        ait = asteroids.erase(ait);
                        pit = projectiles.erase(pit);
                        removed = true;
                        break;
                    }
                }
                if (!removed) {
                    ++pit;
                }
            }

            {
                auto remove_collision =
                    [&allShips, dt](auto& asteroid_ptr_like) -> bool {
                    for (auto* player : allShips) {
                        if (player->IsAlive()) {
                            float dist = Vector2Distance(player->GetPosition(), asteroid_ptr_like->GetPosition());
                            if (dist < player->GetRadius() + asteroid_ptr_like->GetRadius()) {
                                player->TakeDamage(asteroid_ptr_like->GetDamage());
                                return true;
                            }
                        }
                    }
                    if (!asteroid_ptr_like->Update(dt)) {
                        return true;
                    }
                    return false;
                };
                auto asteroid_to_remove = std::remove_if(asteroids.begin(), asteroids.end(), remove_collision);
                asteroids.erase(asteroid_to_remove, asteroids.end());
            }


            player->TrySpawnOrbiter(C_WIDTH, C_HEIGHT, SCORE_THRESHOLD, &sharedTex);

            {
                Renderer::Instance().Begin();

                DrawText(TextFormat("HP: %d", player->GetHP()),
                    10, 10, 20, GREEN);

                const char* weaponName = (currentWeapon == WeaponType::LASER) ? "LASER" : "BULLET";
                DrawText(TextFormat("Weapon: %s", weaponName),
                    10, 40, 20, BLUE);

                DrawText(TextFormat("Score: %d", player->GetScore()),
                    10, 70, 20, YELLOW);

                for (const auto& projPtr : projectiles) {
                    projPtr.Draw();
                }
                for (const auto& astPtr : asteroids) {
                    astPtr->Draw();
                }

                player->Draw();
                player->DrawOrbiters();

                Renderer::Instance().End();
            }
        }
        UnloadTexture(sharedTex);
    }

private:
    Application()
    {
        asteroids.reserve(1000);
        projectiles.reserve(10'000);
    };

    std::vector<std::unique_ptr<Asteroid>> asteroids;
    std::vector<Projectile> projectiles;

    AsteroidShape currentShape = AsteroidShape::TRIANGLE;

    static constexpr int C_WIDTH = 1600;
    static constexpr int C_HEIGHT = 1600;
    static constexpr size_t MAX_AST = 150;
    static constexpr float C_SPAWN_MIN = 0.5f;
    static constexpr float C_SPAWN_MAX = 3.0f;

    static constexpr int C_MAX_ASTEROIDS = 1000;
    static constexpr int C_MAX_PROJECTILES = 10'000;
};

int main() {
	Application::Instance().Run();
	return 0;
}
