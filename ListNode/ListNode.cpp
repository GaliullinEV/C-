#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

using namespace std;

struct ListNode 
{
    ListNode* prev = nullptr;
    ListNode* next = nullptr;
    ListNode* rand = nullptr;
    string data;
};

//читаем наш inlet.in
static ListNode* ReadFile(const string& file) 
{
    ifstream input(file);
    if (!input) 
        return nullptr;

    struct Item 
    {
        ListNode* list;
        int index;
    };

    vector<Item> item;
    string line;
    while (getline(input, line)) {
        //пусть разделителем будет ';'
        size_t pos = line.rfind(';');
        if (pos == string::npos)
            continue;

        string data = line.substr(0, pos);
        int index = stoi(line.substr(pos + 1));

        ListNode* node = new ListNode();
        node->data = data;
        item.push_back({node, index});
    }

    //prev,next
    for (size_t i = 0; i < item.size(); ++i) {
        if (i > 0) {
            item[i].list->prev = item[i - 1].list;
            item[i - 1].list->next = item[i].list;
        }
    }
    //rand
    for (size_t i = 0; i < item.size(); ++i) {
        int value = item[i].index;
        if (value >= 0 && value < static_cast<int>(item.size()))
            item[i].list->rand = item[value].list;
    }
    return item.empty() ? nullptr : item[0].list;
}

//сериализуем
static void serialize(ListNode* file, const string& filename) 
{
    ofstream out(filename, ios::binary);
    if (!out) 
        return;

    vector<ListNode*> list;
    for (ListNode* cur = file; cur != nullptr; cur = cur->next)
        list.push_back(cur);

    int32_t count = static_cast<int32_t>(list.size());
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));

    //записываем data
    for (ListNode* temp : list) {
        int32_t len = static_cast<int32_t>(temp->data.size());
        out.write(reinterpret_cast<const char*>(&len), sizeof(len));
        out.write(temp->data.data(), len);
    }
    //записываем rand
    for (ListNode* temp : list) {
        int32_t idx = -1;

        if (temp->rand != nullptr) {
            for (int32_t i = 0; i < count; ++i) {
                if (list[i] == temp->rand) {
                    idx = i;
                    break;
                }
            }
        }
        out.write(reinterpret_cast<const char*>(&idx), sizeof(idx));
    }
}

//десериализуем
static ListNode* deserialize(const string& filename) 
{
    ifstream in(filename, ios::binary);
    if (!in) 
        return nullptr;

    // Читаем количество узлов
    int32_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!in || count < 0) 
        return nullptr;

    //читаем data
    vector<ListNode*> nods(count, nullptr);
    for (int32_t i = 0; i < count; ++i) {
        int32_t len = 0;

        in.read(reinterpret_cast<char*>(&len), sizeof(len));
        if (!in || len < 0) {
            for (auto* p : nods) 
                delete p;
            return nullptr;
        }

        nods[i] = new ListNode();
        nods[i]->data.resize(len);

        in.read(&nods[i]->data[0], len);
        if (!in) {
            for (auto* p : nods) 
                delete p;
            return nullptr;
        }
    }
    //читаем rand
    vector<int32_t> rand_idx(count, -1);
    for (int32_t i = 0; i < count; ++i) {
        in.read(reinterpret_cast<char*>(&rand_idx[i]), sizeof(rand_idx[i]));
        if (!in) {
            for (auto* p : nods) 
                delete p;
            return nullptr;
        }
    }
    //prev/next
    for (int32_t i = 0; i < count; ++i) {
        if (i > 0) {
            nods[i]->prev = nods[i - 1];
            nods[i - 1]->next = nods[i];
        }
    }
    //rand
    for (int32_t i = 0; i < count; ++i) {
        /*if (rand_idx[i] < 0) 
            nods[i]->rand = nullptr;
        */
        if (rand_idx[i] >= 0 && rand_idx[i] < count) 
            nods[i]->rand = nods[rand_idx[i]];      
        /*else 
            nods[i]->rand = nullptr;
        */
    }
    return count > 0 ? nods[0] : nullptr;
}

//освобождаем память
static void freeList(ListNode* file) 
{
    while (file) 
    {
        ListNode* next = file->next;
        delete file;
        file = next;
    }
}

//выводим output.out (в задании этого не было, но решил написать для проверки)
static void print(ListNode* outfile) 
{
    vector<ListNode*> v;
    for (ListNode* value = outfile; value; value = value->next)
        v.push_back(value);

    for (size_t i = 0; i < v.size(); ++i) 
    {
        cout << i << ": data = " << v[i]->data << ", rand = ";
        if (v[i]->rand) 
        {
            for (size_t j = 0; j < v.size(); ++j) 
            {
                if (v[j] == v[i]->rand)
                {
                    cout << j;
                    break;
                }
            }
        }
        else
            cout << -1;
        cout << "\n";
    }
}

int main() 
{
    setlocale(0, "rus");

    ListNode* FileIn = ReadFile("inlet.in");
    serialize(FileIn, "outlet.out");

    ListNode* FileOut = deserialize("outlet.out");
    print(FileOut);

    freeList(FileIn);
    freeList(FileOut);

    return 0;
}
