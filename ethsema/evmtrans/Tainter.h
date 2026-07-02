//Enum with different types of functions.
enum ApiType {
    API_ENTRY,      //Entry functions like main.
    API_SOURCE,     //Source functions where user input enters the applications like "read" and "recv".
    API_SINK,       //Dangerous functions like "strcpy".
    API_SANITIZE,   //Functions that sanitizes inputs.
    API_USERFUNC    //User defined functions.
};

//Struct to define a function.
struct ApiFunc {
    //In case of source function interestingArg is argument that has user tainted data.
    //In case of sink it's the dengerous argument for example the second argument of strcpy.
    int interestingArg;
    //Function name.
    std::string funcName;
    //Function type.
    ApiType type;
};

//Taint Node, a node represents a function in a taint.
struct TaintNode {
    std::string name;
    int arg;
    ApiType type;
};
bool operator==(const TaintNode& n1, const TaintNode& n2)
{
    if(n1.name == n2.name && n1.arg == n2.arg) {
        return true;
    } else {
        return false;
    }
}
//Represents a taint between two functions.
struct TaintEdge {
    TaintNode src;
    TaintNode dst;
};