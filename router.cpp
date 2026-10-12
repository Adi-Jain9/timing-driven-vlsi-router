#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>

using namespace std;

struct Cell {
    string name;
    string type;
    int x;
    int y;
    int width;
    int height;

};

struct Pin {
    string name;
    double x;
    double y;
};

struct CellType {
    string name;
    int width;
    int height;
    vector<Pin> pins;
};

struct Connection {
    string cellName;
    string pinName;
    string netName;
};

struct AccessPoint {
    string cellName;
    string pinName;
    int x;
    int y;
};

int main() {
    vector<Cell> cells;
    vector<Connection> connections;
    vector<AccessPoint> accessPoints;
    vector<string> criticalNets;

    ifstream criticalFile("../timing_analyzer/critical_nets.txt");

    if(!criticalFile.is_open()){
        cout << "Could not open critical_nets.txt" << endl;
        return 1;
    }

    string netName;

    while(criticalFile >> netName){
        criticalNets.push_back(netName);
    }

    criticalFile.close();

    cout << "Critical Nets:" << endl;

    for(int i = 0; i < criticalNets.size(); i++){
        cout << criticalNets[i] << endl;
    }

    ifstream file("placement.txt");

    if(!file.is_open()){
        cout << "Error: Could not open the placement file" << endl;
        return 1;
    }

    Cell cell;

    while(file >> cell.name >> cell.x >> cell.y){
        cell.width = 0;
        cell.height = 0;
        cells.push_back(cell);
    }

    file.close();

    ifstream lefFile("cell_geometry.lef");

    if(!lefFile.is_open()){
        cout << "Error: Could not open LEF file" << endl;
        return 1;
    }

    vector<CellType> cellTypes;
    CellType currentCell;

    string word;
    Pin currentPin;
    bool readingPin = false;

    while(lefFile >> word){
        if(word == "MACRO"){
            lefFile >> currentCell.name;
            currentCell.pins.clear();
        }

        else if(word == "SIZE"){
            string by;
            lefFile >> currentCell.width >> by >> currentCell.height;
        }
        else if(word == "PIN"){
            lefFile >> currentPin.name;
            readingPin = true;
        }
        else if(word == "RECT" && readingPin){
            double x1, y1, x2, y2;

            lefFile >> x1 >> y1 >> x2 >> y2;

            currentPin.x = (x1 + x2) / 2.0;
            currentPin.y = (y1 + y2) / 2.0;

            currentCell.pins.push_back(currentPin);
        }
        else if(word == "END"){
            string endName;
            getline(lefFile, endName);

            if(readingPin && endName == " " + currentPin.name){
                readingPin = false;
            }
            else if(endName == " " + currentCell.name){
                cellTypes.push_back(currentCell);
            }
        }
    }
    

    lefFile.close();

    ifstream verilogFile("connections.v");

    if(!verilogFile.is_open()){
        cout << "Error: Could not open Verilog file" << endl;
        return 1;
    }

    string vword;

    while(verilogFile >> vword){
        for(int i = 0; i < cellTypes.size(); i++){
            if(vword == cellTypes[i].name){
                string instanceName;
                verilogFile >> instanceName;

                for(int j = 0; j < cells.size(); j++){
                    if(cells[j].name == instanceName){
                        cells[j].type = vword;
                        cells[j].width = cellTypes[i].width;
                        cells[j].height = cellTypes[i].height;
                        break;
                    }
                }

                string pinText;

                while(verilogFile >> pinText){
                    if(pinText == ");"){
                        break;
                    }

                    size_t open = pinText.find('(');
                    size_t close = pinText.find(')');

                    if(pinText[0] == '.' && open < pinText.size() 
                            && close < pinText.size()){
                        
                        Connection connection;

                        connection.cellName = instanceName;
                        connection.pinName = pinText.substr(1, open - 1);

                        connection.netName = 
                            pinText.substr(open + 1, close - open - 1);

                        connections.push_back(connection);
                    }
                }

                break;
            }
        }
    }

    verilogFile.close();

    cout << "\nConnections grouped by net:" << endl;

    for(int i = 0; i < connections.size(); i++){
        bool alreadyPrinted = false;

        for(int j = 0; j < i; j++){
            if(connections[i].netName == connections[j].netName){
                alreadyPrinted = true;
                break;
            }
        }

        if(alreadyPrinted){
            continue;
        }

        cout << "\nNet: " << connections[i].netName << endl;

        for(int j = 0; j < connections.size(); j++){
            if(connections[i].netName == connections[j].netName){
                cout << " "
                     << connections[j].cellName
                     << "."
                     << connections[j].pinName
                     << endl;
            }
        }
    }

    int width = 60;
    int height = 35;

    vector<vector<char>> grid(height, vector<char>(width, '.'));
    vector<vector<bool>> blocked(height, vector<bool>(width, false));

    for(int i = 0; i < cells.size(); i++){
        int x = cells[i].x;
        int y = cells[i].y;

        int cellWidth = cells[i].width;
        int cellHeight = cells[i].height;

        char symbol = 'C';

        if(cells[i].type == "INV_X1"){
            symbol = 'I';
        }
        else if(cells[i].type == "DFF_X1"){
            symbol = 'F';
        }
        else if(cells[i].type == "NAND2_X1"){
            symbol = '2';
        }
        else if(cells[i].type == "NAND3_X1"){
            symbol = '3';
        }

        for(int row = 0; row < cellHeight; row++){
            for(int col = 0; col < cellWidth; col++){
                grid[y + row][x + col] = symbol;
                blocked[y + row][x + col] = true;
            }
        }

    }

    cout << "\nAbsolute Pin Locations:" << endl;
    for(int i = 0; i < cells.size(); i++){
        for(int j = 0; j < cellTypes.size(); j++){
            if(cells[i].type == cellTypes[j].name){
                cout << cells[i].name << ":" << endl;
                for(int k = 0; k < cellTypes[j].pins.size(); k++){
                    double absX = cells[i].x + cellTypes[j].pins[k].x;
                    double absY = cells[i].y + cellTypes[j].pins[k].y;
                    
                    cout << " " << cellTypes[j].pins[k].name
                         << ": (" << absX << ", " << absY << ")"
                         << endl;


                    int pinX = round(absX);
                    int pinY = round(absY);

                    int accessX;
                    int accessY = pinY;

                    if(cellTypes[j].pins[k].x < cells[i].width / 2.0){
                        accessX = cells[i].x - 1;
                    }
                    else{
                        accessX = cells[i].x + cells[i].width;
                    }

                    if(accessX >= 0 && accessX < width && accessY >= 0
                            && accessY < height
                            && !blocked[accessY][accessX]){

                        AccessPoint point;

                        point.cellName = cells[i].name;
                        point.pinName = cellTypes[j].pins[k].name;
                        point.x = accessX;
                        point.y = accessY;

                        accessPoints.push_back(point);
                    }
                    else{
                        cout << "No access point for "
                             << cells[i].name << "."
                             << cellTypes[j].pins[k].name
                             << endl;
                    }

                    if(pinX >= 0 && pinX < width && pinY >= 0 && pinY < height){
                        grid[pinY][pinX] = 'P';
                    }
                }

                break;
            }
        }
    }

    cout << "\nRouting Access Points:" << endl;

    for(int i = 0; i < accessPoints.size(); i++){
        cout << accessPoints[i].cellName
             << "." << accessPoints[i].pinName
             << " -> ("
             << accessPoints[i].x << ", "
             << accessPoints[i].y << ")"
             << endl;
    }

     
    cout << "\nChip Placement:" << endl;

    for(int y = height - 1; y >= 0; y--){
        for(int x = 0; x < width; x++){
            cout << grid[y][x] << " ";
        }

        cout << endl;
    }



    return 0;
}


