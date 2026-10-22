#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <queue>
#include <algorithm>
#include <climits>

using namespace std;

// use these structs to represent cells, pins, connections, etc.

// Represents one physcial cell on the chip
struct Cell {
    string name; // Instance name like "FF1" or "INV1"
    string type; // Cell type like "DFF_X1" or "INV_X1"
    int x; // X-coordinate of the cell's bottom left corner
    int y; // Y-coordinate of the cell's bottom left corner
    int width; // number of grid positions across the cell
    int height; // number of grid positions vertically

};

// Represents one pin belonging to a cell type
struct Pin {
    string name; // pin name like A, B, Y ...
    double x; // Relative horizontal pin position
    double y; // Relative vertical pin position
};

// Definition of a cell from the LEF file
struct CellType {
    string name; // Example: "NAND2_X1"
    int width; // Width of this type of cell
    int height; // Height of this type of cell
    vector<Pin> pins; // List of pins belonging to the cell type
};

// represents one pin connected to a net
struct Connection {
    string cellName; 
    string pinName;
    string netName;
};

// represents a grid position where a wire can access a cell pin
struct AccessPoint {
    string cellName; // Cell containing the pin
    string pinName; // pin being accessed
    int x; // accessible grid x-coordinate
    int y; // accessible grid y-coordinate
};

// represents one drive to desitnation routing problem
// if one net has multiple destinations, it gets multiple tasks
struct RouteTask {
    string netName; // electrical net being routed

    string sourceCell; // cell that drives the net
    string sourcePin; // output pin on that cell

    string targetCell; // destination cell
    string targetPin; // input pin on the destination cell

    int startX; // driver x coordinate
    int startY; // driver y coordinate

    int endX; // destination x coordinate
    int endY; // destination y coordinate

    bool critical; // true is STA marks as critical
};

// represents one coordinate on the routing grid
struct GridPoint {
    int x; 
    int y;
};

// represents one candidate position while running dijkstra
struct SearchNode {
    int distance; // cheapest known cost to reach this position
    int x; //candidate x coordinate
    int y; //candidate y coordinate
};

// Dijkstra needs to explore the lowest cost candidates first.
// A normal priority queue goes largets first but this makes it go smallest first
struct CompareNodes {

    // return true when node a has a greater cost than b
    // gives b higher priority because b is cheaper
    bool operator()(SearchNode a, SearchNode b){
        return a.distance > b.distance;
    }
};

// this function tries to find the cheapest path from an existing routing
// tree to a new destination
//
// this starts from any point in an existing net tree.
//
// Parameters:
//
// tree:
//      Points already connected together for this electrical net
// 
// target:
//      the destination coordinate we want to reach
//
// blocked:
//      blocked[y][x] is true if a standard cell occopues the grid
// 
// owner:
//      owner[y][x] stores which net owns a grid position
//      -1 means unoccupied
//
// congestionWeight:
//      Determines how strongly we avoid nearby wires
//
// path:
//      Stores the route found by Dijkstra
//
// returns:
//      true = successfully found a path
//      false = no valid path exists with current occupied positions

bool dijkstraToTree(
        vector<GridPoint>& tree,
        GridPoint target,
        vector<vector<bool>>& blocked,
        vector<vector<int>>& owner,
        int netId,
        int congestionWeight,
        vector<GridPoint>& path){

    // Determines the grids dimensions
    int height = blocked.size();
    int width = blocked[0].size();

    int INF = 100000000;

    // stores the best routing cost discovered so far for reaching position (x, y).
    vector<vector<int>> distance(height, vector<int>(width, INF));

    // records whether Dijkstra has permanently processed this position
    vector<vector<bool>> visited(height, vector<bool>(width, false));

    // stores the previous coordinate we came from through the cheapest route
    vector<vector<GridPoint>> previous(height, vector<GridPoint>(width));

    // initalize every predecessor to (-1, -1)
    for(int y = 0; y < height; y++){
        for(int x = 0; x < width; x++){
            previous[y][x].x = -1;
            previous[y][x].y = -1;
        }
    }

    // create the priority queue
    // SearchNode = the objects stored in the queue
    // vector<SearchNode> = internal storage
    // CompareNodes = makes the smallest distance come first
    priority_queue<
        SearchNode,
        vector<SearchNode>,
        CompareNodes
    > pq;

    // start from every point of the existing net tree
    for(int i = 0; i < tree.size(); i++){
        int x = tree[i].x;
        int y = tree[i].y;

        distance[y][x] = 0;

        SearchNode node;
        node.distance = 0;
        node.x = x;
        node.y = y;

        pq.push(node);
    }

    // no diagonal movement is allowed
    int dx[4] = {1, -1, 0, 0};
    int dy[4] = {0, 0, 1, -1};

    // keep searching while candidates remain in the queue
    while(!pq.empty()){

        // get the candidate with the lowest routing cost
        SearchNode current = pq.top();
        pq.pop();

        int x = current.x;
        int y = current.y;

        // check if position is already processed
        if(visited[y][x]){
            continue;
        }

        visited[y][x] = true;

        // check if cheapest route is already found
        if(x == target.x && y == target.y){
            path.clear();

            GridPoint point;
            point.x = x;
            point.y = y;

            while(point.x != -1){
                path.push_back(point);

                GridPoint next = previous[point.y][point.x];

                point = next;
            }

            reverse(path.begin(), path.end());

            return true;
        }

        for(int dir = 0; dir < 4; dir++){

            // gets the neighbors coordinates 
            int nx = x + dx[dir];
            int ny = y + dy[dir];

            // skips if outside the grid
            if(nx < 0 || nx >= width || ny < 0 || ny >= height){
                continue;
            }

            // skip is already processed
            if(visited[ny][nx]){
                continue;
            }

            // skip positions occupied by standard cell
            if(blocked[ny][nx]){
                continue;
            }

            // skip positions owned by a different electrical net
            if(owner[ny][nx] != -1 && owner[ny][nx] != netId){
                continue;
            }

            int congestion = 0;

            for(int k = 0; k < 4; k++){
                int nearbyX = nx + dx[k];
                int nearbyY = ny + dy[k];

                if(nearbyX < 0 || nearbyX >= width || nearbyY < 0 ||
                        nearbyY >= height){
                    continue;
                }

                // if a adjacent position belongs to anotherr net then
                // increase the congestion count
                if(owner[nearbyY][nearbyX] != -1 &&
                        owner[nearbyY][nearbyX] != netId){
                    congestion++;
                }
            }

            // every movement has a basic cost of 1
            //
            // stepCost = 1 + congestionWeight * congestion

            int stepCost = 1 + congestionWeight * congestion;
            int newDistance = distance[y][x] + stepCost;

            if(newDistance < distance[ny][nx]){
                distance[ny][nx] = newDistance;

                previous[ny][nx].x = x;
                previous[ny][nx].y = y;

                SearchNode nextNode;
                nextNode.distance = newDistance;
                nextNode.x = nx;
                nextNode.y = ny;

                pq.push(nextNode);
            }
        }
    }

    path.clear();
    return false;
}


// helps indentify the output pins
bool isOutputPin(string pinName){
    return pinName == "Y" || pinName == "Q";
}

// Load design files, build the grid, and route connections.
int main() {
    vector<Cell> cells; // stores all placed cell positions
    vector<Connection> connections; // stores the pin-to-net connections
    vector<AccessPoint> accessPoints; // stores grid locations where wires can access pins
    vector<string> criticalNets; // critical cells from STA
    vector<RouteTask> routeTasks; // stores driver to destination connections

    // open the file from the python STA
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
    
    // Load cell positions from placement.txt.
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

    // Parse LEF for cell dimensions and pin locations.
    ifstream lefFile("cell_geometry.lef");

    if(!lefFile.is_open()){
        cout << "Error: Could not open LEF file" << endl;
        return 1;
    }

    vector<CellType> cellTypes; // stores the different cell types in the LEF file
    CellType currentCell; // stores the current cell type being read

    string word; // stores the current word from the file
    Pin currentPin; // stores the pin currently being read
    bool readingPin = false;

    while(lefFile >> word){
        // The beginning of a cell-type definition
        if(word == "MACRO"){
            lefFile >> currentCell.name;
            currentCell.pins.clear();
        }

        // tells us the cell width and height
        else if(word == "SIZE"){
            string by;
            lefFile >> currentCell.width >> by >> currentCell.height;
        }

        // the beginning of a pin definition
        else if(word == "PIN"){
            lefFile >> currentPin.name;
            readingPin = true;
        }

        // provides the rectangluar geometry of a pin
        else if(word == "RECT" && readingPin){
            double x1, y1, x2, y2;

            lefFile >> x1 >> y1 >> x2 >> y2;

            // approximate the pin position using its center
            currentPin.x = (x1 + x2) / 2.0;
            currentPin.y = (y1 + y2) / 2.0;

            currentCell.pins.push_back(currentPin);
        }

        // end marks the end of a pin or cell definition
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

    // the verilog netlist tells how the pins are connected
    //
    // tells which pins must be connected
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


    // dimensions of the routing grid
    int width = 70;
    int height = 35;

    // creates the grid with dots
    vector<vector<char>> grid(height, vector<char>(width, '.'));

    // marks each grid point as not blocked
    vector<vector<bool>> blocked(height, vector<bool>(width, false));

    // creates the symbols for each of the type of cells
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

    // calculates the absolute pin coordinates and routing access points
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

    // generate driver-to-destination routing tasks
    for(int i = 0; i < connections.size(); i++){
        if(!isOutputPin(connections[i].pinName)){
            continue;
        }

        for(int j = 0; j < connections.size(); j++){
            if(connections[i].netName != connections[j].netName ||
                isOutputPin(connections[j].pinName)){
                continue;
            }

            RouteTask task;

            task.netName = connections[i].netName;
            task.sourceCell = connections[i].cellName;
            task.sourcePin = connections[i].pinName;
            task.targetCell = connections[j].cellName;
            task.targetPin = connections[j].pinName;

            task.startX = -1;
            task.startY = -1;
            task.endX = -1;
            task.endY = -1;
            task.critical = false;

            for(int crit = 0; crit < criticalNets.size(); crit++){
                if(task.netName == criticalNets[crit]){
                    task.critical = true;
                    break;
                }
            }

            for(int k = 0; k < accessPoints.size(); k++){
                if(accessPoints[k].cellName == task.sourceCell &&
                    accessPoints[k].pinName == task.sourcePin){

                    task.startX = accessPoints[k].x;
                    task.startY = accessPoints[k].y;
                }

                if(accessPoints[k].cellName == task.targetCell &&
                    accessPoints[k].pinName == task.targetPin){

                    task.endX = accessPoints[k].x;
                    task.endY = accessPoints[k].y;
                }
            }

            if(task.startX == -1 || task.startY == -1 || task.endX == -1 ||
                    task.endY == -1){
                cout << "Missing access point for net "
                     << task.netName << endl;
                continue;
            }

            routeTasks.push_back(task);
        }
    }

    // sort critical paths first then by Manhattan distance
    for(int i = 0; i < routeTasks.size(); i++){
        int best = i;

        for(int j = i + 1; j < routeTasks.size(); j++){
            if(routeTasks[j].critical && !routeTasks[best].critical){
                best = j;
            }
            else if(routeTasks[j].critical == routeTasks[best].critical){

                int distanceJ = 
                    abs(routeTasks[j].endX - routeTasks[j].startX) +
                    abs(routeTasks[j].endY - routeTasks[j].startY);

                int distanceBest = 
                    abs(routeTasks[best].endX - routeTasks[best].startX) +
                    abs(routeTasks[best].endY - routeTasks[best].startY);

                if(distanceJ < distanceBest){
                    best = j;
                }
            }
        
        }

        RouteTask temp = routeTasks[i];
        routeTasks[i] = routeTasks[best];
        routeTasks[best] = temp;
    }


    cout << "\nRouting Tasks:" << endl;

    for(int i = 0; i < routeTasks.size(); i++){
        cout << routeTasks[i].netName << ": "
            << routeTasks[i].sourceCell << "."
            << routeTasks[i].sourcePin
            << " (" << routeTasks[i].startX
            << ", " << routeTasks[i].startY << ") -> "
            << routeTasks[i].targetCell << "."
            << routeTasks[i].targetPin
            << " (" << routeTasks[i].endX
            << ", " << routeTasks[i].endY << ")"
            << " | Critical: "
            << (routeTasks[i].critical ? "Yes" : "No")
            << endl;
    }

    vector<string> routedNetNames; // assign a unique integer ID to each net

    for(int i = 0; i < routeTasks.size(); i++){
        bool found = false;

        for(int j = 0; j < routedNetNames.size(); j++){
            if(routeTasks[i].netName == routedNetNames[j]){
                found = true;
                break;
            }
        }

        if(!found){
            routedNetNames.push_back(routeTasks[i].netName);
        }
    }

    // Track which net owns each grid position
    vector<vector<int>> owner(height, vector<int>(width, -1));
    vector<bool> validTask(routeTasks.size(), true);

    // validate and reserve surce and destination points
    for(int i = 0; i < routeTasks.size(); i++){
        int netId = -1;

        for(int j = 0; j < routedNetNames.size(); j++){
            if(routeTasks[i].netName == routedNetNames[j]){
                netId = j;
                break;
            }
        }

        int x[2] = {routeTasks[i].startX, routeTasks[i].endX};
        int y[2] = {routeTasks[i].startY, routeTasks[i].endY};

        for(int k = 0; k < 2; k++){
            if(x[k] < 0 || x[k] >= width || y[k] < 0 || y[k] >= height){
                validTask[i] = false;
                break;
            }
        
            if(blocked[y[k]][x[k]]){
                validTask[i] = false;
                break;
            }

            if(owner[y[k]][x[k]] != -1 && owner[y[k]][x[k]] != netId){
                validTask[i] = false;
                break;
            }
        }

        if(!validTask[i]){
            cout << "Invalid endpoints for " << routeTasks[i].netName << endl;
            continue;
        }

        owner[routeTasks[i].startY][routeTasks[i].startX] = netId;
        owner[routeTasks[i].endY][routeTasks[i].endX] = netId;
    }

    // store connected routing trees for multi-terminal nets
    vector<vector<GridPoint>> trees(routedNetNames.size());

    int congestionWeight = 2;

    int successfulTasks = 0;
    int failedTasks = 0;
    int totalWireLength = 0;

    cout << "\nDijkstra Routing Results:" << endl;

    // route each net using congestion aware Dijkstra
    for(int netId = 0; netId < routedNetNames.size(); netId++){
        for(int i = 0; i < routeTasks.size(); i++){
            if(routeTasks[i].netName != routedNetNames[netId]){
                continue;
            }

            RouteTask task = routeTasks[i];

            if(!validTask[i]){
                failedTasks++;
                cout << "FAILED: " << task.netName << " (Invalid endpoint)"
                     << endl;
                continue;
            }

            if(trees[netId].empty()){
                GridPoint source;
                source.x = task.startX;
                source.y = task.startY;

                trees[netId].push_back(source);
            }

            GridPoint target;
            target.x = task.endX;
            target.y = task.endY;

            vector<GridPoint> path;

            // search for a valid path to the current destination
            bool found = dijkstraToTree(trees[netId], target, blocked, owner,
                                            netId, congestionWeight, path);

            if(!found){
                failedTasks++;

                cout << "FAILED: "
                     << task.netName << " -> "
                     << task.targetCell << "."
                     << task.targetPin << endl;

                continue;
            }

            // save successful paths and update the grid
            for(int k = 0; k < path.size(); k++){
                int x = path[k].x;
                int y = path[k].y;

                owner[y][x] = netId;;

                trees[netId].push_back(path[k]);

                if(grid[y][x] == '.'){
                    grid[y][x] = '*';
                }
            }

            int length = path.size() - 1;

            totalWireLength += length;
            successfulTasks++;

            if(task.critical){
                cout << "CRITICAL: ";
            }
            else{
                cout << "NORMAL: ";
            }

            cout << task.netName << " -> "
                 << task.targetCell << "."
                 << task.targetPin
                 << " | Length: " << length
                 <<endl;
        }
    }

    cout << "\nRouting Summary:" << endl;

    cout << "Successful tasks: " << successfulTasks << endl;

    cout << "Failed tasks: " << failedTasks << endl;

    cout << "Total tasks: " << routeTasks.size() << endl;

    cout << "Total added wire length: " << totalWireLength
         << " grid steps" << endl;
    
    cout << "\nChip Placement:" << endl;

    for(int y = height - 1; y >= 0; y--){
        for(int x = 0; x < width; x++){
            cout << grid[y][x] << " ";
        }

        cout << endl;
    }



    return 0;
}


