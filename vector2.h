typedef struct { float x, y; } Vector2;


Vector2 add_vectors (Vector2 a, Vector2 b) {
    Vector2 c;
    c.x = a.x + b.x;
    c.y = a.y + b.y;
    return c;
}


Vector2 mult_vector (Vector2 a, float b) {
    Vector2 c;
    c.x = a.x * b;
    c.y = a.y * b;
    return c;
}