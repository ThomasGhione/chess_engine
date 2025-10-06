class Piece {
    public:
        Piece() : id(EMPTY) {}
        Piece(coords c, piece_id i) : coords(c), id(i) {}
    
    protected:
        Coords coords;
        piece_id id;
}