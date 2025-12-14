#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include <memory>
#include <chrono>

using namespace std;
using namespace sf;

// Struct buat nyimpen data bola
struct Ball {
    CircleShape shape;
    Vector2f velocity;
    Vector2f center;

    Ball(float radius, Vector2f pos, Vector2f vel, Color color) {
        shape.setRadius(radius);
        shape.setFillColor(color);
        shape.setPosition(pos);
        velocity = vel;
        updateCenter();
    }

    void updateCenter() {
        float r = shape.getRadius();
        Vector2f pos = shape.getPosition();
        center = Vector2f(pos.x + r, pos.y + r);
    }

    float getRadius() const {
        return shape.getRadius();
    }
};

// Struct buat area boundary (dipake di QuadTree)
struct Boundary {
    float x, y, w, h;

    Boundary(float x, float y, float w, float h) : x(x), y(y), w(w), h(h) {}

    bool contains(const Vector2f& point) const {
        return (point.x >= x && point.x <= x + w &&
                point.y >= y && point.y <= y + h);
    }

    bool intersects(const Boundary& range) const {
        return !(range.x > x + w || range.x + range.w < x ||
                 range.y > y + h || range.y + range.h < y);
    }
};

// Class QuadTree buat optimasi collision detection
class QuadTree {
private:
    Boundary boundary;
    int capacity;
    vector<Ball*> balls;
    bool divided;

    unique_ptr<QuadTree> northwest;
    unique_ptr<QuadTree> northeast;
    unique_ptr<QuadTree> southwest;
    unique_ptr<QuadTree> southeast;

    // Fungsi buat bagi area jadi 4 bagian
    void subdivide() {
        float x = boundary.x;
        float y = boundary.y;
        float w = boundary.w / 2;
        float h = boundary.h / 2;

        northwest = make_unique<QuadTree>(Boundary(x, y, w, h), capacity);
        northeast = make_unique<QuadTree>(Boundary(x + w, y, w, h), capacity);
        southwest = make_unique<QuadTree>(Boundary(x, y + h, w, h), capacity);
        southeast = make_unique<QuadTree>(Boundary(x + w, y + h, w, h), capacity);

        divided = true;
    }

public:
    QuadTree(Boundary boundary, int capacity)
        : boundary(boundary), capacity(capacity), divided(false) {}

    // Masukin bola ke dalam tree
    bool insert(Ball* ball) {
        if (!boundary.contains(ball->center)) {
            return false;
        }

        if (balls.size() < capacity && !divided) {
            balls.push_back(ball);
            return true;
        }

        if (!divided) {
            subdivide();
        }

        if (northwest->insert(ball)) return true;
        if (northeast->insert(ball)) return true;
        if (southwest->insert(ball)) return true;
        if (southeast->insert(ball)) return true;

        return false;
    }

    // Cari bola-bola yang ada di area tertentu
    vector<Ball*> query(const Boundary& range) {
        vector<Ball*> found;

        if (!boundary.intersects(range)) {
            return found;
        }

        for (auto* ball : balls) {
            if (range.contains(ball->center)) {
                found.push_back(ball);
            }
        }

        if (divided) {
            auto nw = northwest->query(range);
            auto ne = northeast->query(range);
            auto sw = southwest->query(range);
            auto se = southeast->query(range);

            found.insert(found.end(), nw.begin(), nw.end());
            found.insert(found.end(), ne.begin(), ne.end());
            found.insert(found.end(), sw.begin(), sw.end());
            found.insert(found.end(), se.begin(), se.end());
        }

        return found;
    }

};

// Cek apakah 2 bola bertabrakan (pake jarak)
bool checkCollision(Ball* a, Ball* b) {
    float dx = a->center.x - b->center.x;
    float dy = a->center.y - b->center.y;
    float dist = sqrt(dx * dx + dy * dy);
    float minDist = a->getRadius() + b->getRadius();
    return dist < minDist;
}

// Handle collision dengan separasi bola
void resolveCollision(Ball* a, Ball* b) {
    // Hitung jarak antar pusat bola
    float dx = b->center.x - a->center.x;
    float dy = b->center.y - a->center.y;
    float dist = sqrt(dx * dx + dy * dy);

    // Overlap distance
    float overlap = (a->getRadius() + b->getRadius()) - dist;

    // Pisahin bola supaya ga nempel
    if (dist > 0) {
        float separateX = (dx / dist) * overlap * 0.5f;
        float separateY = (dy / dist) * overlap * 0.5f;

        // Geser bola A dan B berlawanan arah
        Vector2f posA = a->shape.getPosition();
        Vector2f posB = b->shape.getPosition();

        a->shape.setPosition(Vector2f(posA.x - separateX, posA.y - separateY));
        b->shape.setPosition(Vector2f(posB.x + separateX, posB.y + separateY));

        a->updateCenter();
        b->updateCenter();
    }

    // Tuker velocity (pantulan)
    swap(a->velocity, b->velocity);
}

// Metode 1: Brute Force - cek semua bola satu per satu
// Kompleksitas: O(n^2)
int bruteForceCollision(vector<Ball>& balls) {
    int collisions = 0;

    // Nested loop, cek semua pasangan bola
    for (size_t i = 0; i < balls.size(); i++) {
        for (size_t j = i + 1; j < balls.size(); j++) {
            if (checkCollision(&balls[i], &balls[j])) {
                resolveCollision(&balls[i], &balls[j]);
                collisions++;
            }
        }
    }

    return collisions;
}

// Metode 2: Pake QuadTree - lebih efisien
// Kompleksitas: O(n log n) average case
int quadTreeCollision(vector<Ball>& balls, QuadTree& qtree) {
    int collisions = 0;

    for (auto& ball : balls) {
        float r = ball.getRadius();

        // Bikin area pencarian di sekitar bola
        Boundary range(
            ball.center.x - r * 2,
            ball.center.y - r * 2,
            r * 4,
            r * 4
        );

        // Cari bola-bola yang deket aja
        vector<Ball*> nearby = qtree.query(range);

        for (auto* other : nearby) {
            if (&ball != other && checkCollision(&ball, other)) {
                resolveCollision(&ball, other);
                collisions++;
            }
        }
    }

    // Dibagi 2 karena setiap collision kehitung 2x
    return collisions / 2;
}

int main() {
    // Setting window
    const int WIDTH = 1200;
    const int HEIGHT = 800;
    RenderWindow window(VideoMode({WIDTH, HEIGHT}), "DEMO STRUKDAT");
    window.setFramerateLimit(60);

    vector<Ball> balls;
    int numBalls = 50;
    bool useQuadTree = false;

    // Fungsi buat bikin bola random
    auto createRandomBall = [&]() {
        float x = rand() % (WIDTH - 40) + 20;
        float y = rand() % (HEIGHT - 40) + 20;
        Vector2f pos(x, y);
        Vector2f vel(
            (rand() % 5 + 1) * (rand() % 2 ? 1 : -1),
            (rand() % 5 + 1) * (rand() % 2 ? 1 : -1)
        );
        Color color(rand() % 256, rand() % 256, rand() % 256);
        balls.emplace_back(15.f, pos, vel, color);
        numBalls++;
    };

    // Bikin bola-bola random
    for (int i = 0; i < numBalls; i++) {
        float x = rand() % (WIDTH - 40) + 20;
        float y = rand() % (HEIGHT - 40) + 20;
        Vector2f pos(x, y);
        Vector2f vel(
            (rand() % 5 + 1) * (rand() % 2 ? 1 : -1),
            (rand() % 5 + 1) * (rand() % 2 ? 1 : -1)
        );
        Color color(rand() % 256, rand() % 256, rand() % 256);
        balls.emplace_back(15.f, pos, vel, color);
    }

    // Load font buat text
    Font font;
    bool fontLoaded = false;
    if (font.openFromFile("C:/Windows/Fonts/arial.ttf")) {
        fontLoaded = true;
    } else if (font.openFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) {
        fontLoaded = true;
    }

    Text infoText(font);
    infoText.setCharacterSize(20);
    infoText.setFillColor(Color::White);
    infoText.setPosition({10.f, 10.f});

    long long bruteTime = 0, quadTime = 0;
    int collisionCount = 0;

    // Game loop
    while (window.isOpen()) {

        // Handle event
        while (auto event = window.pollEvent()) {
            if (event->is<Event::Closed>()) {
                window.close();
            }

            if (event->is<Event::KeyPressed>()) {
                auto keyEvent = event->getIf<Event::KeyPressed>();
                if (keyEvent->code == Keyboard::Key::Space) {
                    useQuadTree = !useQuadTree;
                }
            }

            if (event->is<Event::MouseButtonPressed>()) {
                auto mouseEvent = event->getIf<Event::MouseButtonPressed>();
                if (mouseEvent->button == Mouse::Button::Right) {
                    createRandomBall();
                }
            }
        }

        // Update posisi semua bola
        for (auto& b : balls) {
            b.shape.move(b.velocity);
            b.updateCenter();

            float x = b.shape.getPosition().x;
            float y = b.shape.getPosition().y;
            float r = b.shape.getRadius();

            // Kalo nabrak tepi, pantul balik
            if (x < 0) {
                b.shape.setPosition(Vector2f(0.f, y));
                b.velocity.x *= -1;
                b.updateCenter();
            } else if (x + 2*r > WIDTH) {
                b.shape.setPosition(Vector2f(WIDTH - 2*r, y));
                b.velocity.x *= -1;
                b.updateCenter();
            }

            if (y < 0) {
                b.shape.setPosition(Vector2f(x, 0.f));
                b.velocity.y *= -1;
                b.updateCenter();
            } else if (y + 2*r > HEIGHT) {
                b.shape.setPosition(Vector2f(x, HEIGHT - 2*r));
                b.velocity.y *= -1;
                b.updateCenter();
            }
        }

        // Collision detection
        QuadTree qtree(Boundary(0, 0, WIDTH, HEIGHT), 4);

        if (useQuadTree) {
            auto start = chrono::high_resolution_clock::now();

            for (auto& ball : balls) {
                qtree.insert(&ball);
            }

            collisionCount = quadTreeCollision(balls, qtree);

            auto end = chrono::high_resolution_clock::now();
            quadTime = chrono::duration_cast<chrono::microseconds>(end - start).count();
        } else {
            auto start = chrono::high_resolution_clock::now();
            collisionCount = bruteForceCollision(balls);
            auto end = chrono::high_resolution_clock::now();
            bruteTime = chrono::duration_cast<chrono::microseconds>(end - start).count();
        }

        // Mulai render
        window.clear(Color(20, 20, 30));

        // Gambar semua bola
        for (auto& b : balls) {
            window.draw(b.shape);
        }

        // Gambar info text
        if (fontLoaded) {
            string info = "Mode: " + string(useQuadTree ? "QuadTree" : "Brute Force") + "\n";
            info += "Bola: " + to_string(numBalls) + "\n";
            info += "Tabrakan: " + to_string(collisionCount) + "\n";
            info += "Waktu: " + to_string(useQuadTree ? quadTime : bruteTime) + " ms\n\n";
            info += "[SPASI] Ganti Mode\n";
            info += "[KLIK KANAN] Spawn Bola";

            infoText.setString(info);
            window.draw(infoText);
        }

        window.display();
    }

    return 0;
}
