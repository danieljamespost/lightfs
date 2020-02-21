// internal
#include "cli/cli.h"

// system
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

/*
 * start() takes the argument count and the argument vector passed in from
 * `main()` and calls the functions that correspond with their commands.
 */
int CLI::start(int argc, char* argv[])
{
    checkConfig();
    _db->setLocation(_cf->getDBLocation());
    checkDB();

    if (_db->shouldInit() == true) {
        std::cout << "initializing database configuration...  ";
        _db->init();
        std::cout << "done." << std::endl;
        std::cout << "\n";
    }

    if (argc == 1) {
        std::cout << "no command supplied.\n" << std::endl;
        printUsage();
        std::cout << "\n";
        return 1;
    }

    std::string arg(argv[1]);
    switch (_args[arg]) {
    case ADD:

        if (argc != 3) {
            std::cout << "please supply a path to the [add] command.\n"
                      << std::endl;
            printUsage();
            std::cout << "\n";
            return 1;
        }

        handleAdd(argv[2]);
        break;
    case LIST:
        handleList();
        break;
    case REMOVE:
        if (argc != 3) {
            std::cout << "please supply a hash to the [rm] command.\n"
                      << std::endl;
            printUsage();
            std::cout << "\n";
            return 1;
        }
        handleRemove(argv[2]);
        break;
    case SERVE:
        if (argc != 3) {
            std::cout << "please supply a hash to the [serve] command.\n"
                      << std::endl;
            printUsage();
            std::cout << "\n";
            return 1;
        }
        handleServe(argv[2]);
        break;
    case HELP:
        printUsage();
        break;
    default:

        std::cout << arg << " is not a valid command.\n" << std::endl;
        printUsage();
        std::cout << "\n";
        return 1;
    }

    return 0;
}

/*
 * `checkConfig()` determines whether or not it should initialize the users
 * machine with the default configuration. This operation occurs on the first
 * time the user runs the `lfs` command.
 */
void CLI::checkConfig()
{
    if (_cf->shouldInit() == true) {
        std::cout << "\n";
        std::cout << "initializing configuration...  ";
        _cf->init();
        _db->setLocation(_cf->getDBLocation());
        std::cout << "done." << std::endl;
    }
}

/*
 * `checkDB()` determines wheter or not to initialize the users machine with the
 * json database containing the stored state of the content that has been
 * hashed.
 */
void CLI::checkDB()
{
    if (_db->shouldInit() == true) {
        std::cout << "initializing database configuration...  ";
        _db->init();
        std::cout << "done." << std::endl;
        std::cout << "\n";
    }
}

void CLI::printUsage()
{
    std::cout << "usage: lfs <command> <args>" << std::endl;
    std::cout << "\n";
    std::cout << "These are the common lfs commands:" << std::endl;
    std::cout << "\n";
    std::cout << "    add    <directory>   Add directory to the local database "
                 "of verified hashed contents."
              << std::endl;
    std::cout << "\n";
    std::cout << "    status               List the hashed content that is "
                 "currently stored."
              << std::endl;
    std::cout << "\n";
    std::cout << "    rm     <hash>        Remove the content with selected "
                 "hash from database."
              << std::endl;
    std::cout << "\n";
    std::cout << "    serve  <hash>        Serve the content with specified "
                 "hash for network access"
              << std::endl;
    std::cout << "\n";
    std::cout << "    help                 Show usage message." << std::endl;
    std::cout << "\n";
}

/*
 * `handleAdd()` takes the path the user submitted and constructs a `MerkleTree`
 * from the files in that directory.
 */
void CLI::handleAdd(std::basic_string<char> input)
{
    std::string path(input);
    std::string absPath = fs::absolute(path).string();
    std::vector<Content> list = getContentListForPath(absPath);

    MerkleTree t = constructMerkleTree(list);
    persist(list, t.getMerkleRoot(), absPath);

    std::string root = t.getMerkleRoot();
    std::cout << "\n";

    if (t.verify() == false) {
        std::cout << "error: tree is invalid." << std::endl;
        return;
    }
    std::cout << "root hash: " << root << std::endl;
    std::cout << "tree size: " << t.getSize() << std::endl;

    for (int i = 0; i < list.size(); i++) {
        Content c = list[i];
        std::cout << "file: " << c.getPath() << std::endl;
        std::cout << "      " << c.calculateHash() << std::endl;
        std::cout << "\n";
    }
}

/*
 * `getContentListForPath()` takes the path to a directory and returns a vector
 * of `Content` objects for the files contained in that directory.
 */
std::vector<Content> CLI::getContentListForPath(std::string path)
{
    // read all file names in directory
    std::vector<Content> list;
    for (auto& entry : fs::recursive_directory_iterator(path)) {
        if (!fs::is_directory(entry)) {
            Content c = Content(fs::absolute(entry.path()).string());
            list.push_back(c);
        }
    }
    return list;
}

/*
 * `constructMerkleTree()` wraps the `MerkleTree` library and constructs the
 * tree
 */
MerkleTree CLI::constructMerkleTree(std::vector<Content> list)
{
    MerkleTree t = MerkleTree(list);
    return t;
}

/*
 * `persist()` takes the metadata from the `MerkleTree` and persists its state
 * in the users local json database
 */
void CLI::persist(std::vector<Content> c, std::string root, std::string path)
{
    nlohmann::json j = _db->readAll();

    struct root e;
    e.root_path = path;
    for (auto content : c) {
        struct elem l;
        l.content_hash = content.calculateHash();
        l.content_path = content.getPath();
        e.contents.push_back(l);
    }

    j[root] = _db->toJson(e);
    ;
    _db->write(j);
}

/*
 * `handleRemove()` removes the state tree with the given hash
 */
void CLI::handleRemove(std::basic_string<char> input)
{
    std::string hash(input);
    nlohmann::json j = _db->readAll();
    j.erase(hash);
    _db->write(j);

    std::cout << "\n";
    std::cout << "removed hash: " << hash << std::endl;
    std::cout << "\n";
}

/*
 * `handleList()` lists the stored state trees in the users local json database
 */
void CLI::handleList()
{
    nlohmann::json j = _db->readAll();
    std::cout << "\n";
    if (j.is_null() == true) {
        std::cout << "no files have been added" << std::endl;
    } else {
        std::cout << j.dump(4) << std::endl;
    }
    std::cout << "\n";
}

/*
 * `handleServe()` retrieves the state tree from the local json database,
 * verifies the content, and serves it for the user to view in their web
 * browser.
 */
void CLI::handleServe(std::basic_string<char> input)
{
    std::string hash(input);
    nlohmann::json j = _db->readAll();

    // returns an array of stored json objects
    nlohmann::json contents = j[hash].get<nlohmann::json>();
    struct root r = _db->fromJson(contents);

    std::cout << "\n";
    std::cout << "verifying contents of hash " << hash << std::endl;

    if (verifyContents(r, hash) == false) {
        std::cout << "\n";
        std::cout << "contents have been changed since being constructed."
                  << std::endl;
        std::cout << "please run 'lfs add <path>' to add the changed contents"
                  << std::endl;
        std::cout << "\n";
    } else {
        // initialize server settings
        std::string host = "localhost";
        int port = 3000;
        std::vector<endpoint> endpoints = getEndpoints(host, port, r, hash);

        Server svr = Server(host, port);
        svr.start(endpoints);
    }
}

/*
 * `getEndpoints()` constructs a vector of `endpoint` objects containing the
 * html elements derived from the `Content` objects for the given root hash
 */
std::vector<endpoint> CLI::getEndpoints(
    std::string host, int port, root r, std::string hash)
{
    std::vector<endpoint> endpoints;

    // construct root html page
    struct endpoint root;
    root.path = "/" + hash;
    std::stringstream content;

    // start root page html
    content << "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>"
            << hash << "</title></head><body> ";

    // add to root page html and construct individual content pages
    for (auto c : r.contents) {

        // read contents of text file into string
        std::ifstream input(c.content_path);
        std::string textContent((std::istreambuf_iterator<char>(input)),
            (std::istreambuf_iterator<char>()));

        std::stringstream html;
        html << "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>"
             << c.content_hash << "</title></head><body> "
             << "<div>" << textContent << "</div>"
             << "</body></html>";

        struct endpoint e;
        e.path = "/" + c.content_hash;
        e.content = html.str();
        e.content_type = "text/html";

        endpoints.push_back(e);

        // add content to html elements for root page
        content << "<div>"
                << "<a href='"
                << "http://" << host << ":" << port << e.path << "'>"
                << c.content_hash << "</a>"
                << "</div>";
    }

    // end root page html
    content << "</body></html>";

    root.content = content.str();
    root.content_type = "text/html";

    std::cout << "\n";
    std::cout << "serving root at "
              << "http://" << host << ":" << port << "/" << hash << std::endl;
    std::cout << "\n";

    endpoints.push_back(root);

    return endpoints;
}

/*
 * `verifyContents()` ensures that the contents haven't changed since the user
 * added the state tree
 */
bool CLI::verifyContents(root r, std::string hash)
{
    std::vector<Content> contents = getContentListForPath(r.root_path);
    MerkleTree tree = constructMerkleTree(contents);
    return tree.getMerkleRoot() == hash;
}
