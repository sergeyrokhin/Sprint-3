#include <iostream>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

/* Подставьте вашу реализацию класса SearchServer сюда */
const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            words.push_back(word);
            word = "";
        } else {
            word += c;
        }
    }
    words.push_back(word);
    
    return words;
}
    
struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }    
    
    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, 
            DocumentData{
                ComputeAverageRating(ratings), 
                status
            });
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {            
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);
        
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {            
        return FindTopDocuments(raw_query, []([[maybe_unused]] int document_id, [[maybe_unused]] DocumentStatus status, [[maybe_unused]] int rating) { return status == DocumentStatus::ACTUAL; });
    }

    int GetDocumentCount() const {
        return documents_.size();
    }
    
    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }
    
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }
    
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }
    
    static int ComputeAverageRating(const vector<int>& ratings) {
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }
    
    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };
    
    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }
    
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };
    
    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }
    
    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
            {
                const auto &document_data = documents_.at(document_id);

                bool is_selected = false;
                if constexpr (is_same_v<DocumentPredicate, DocumentStatus>)
                {
                    if (document_predicate == document_data.status)
                    {
                        is_selected = true;
                    }
                }
                else
                {
                    if (document_predicate(document_id, document_data.status, document_data.rating))
                    {
                        is_selected = true;
                    }
                }
                if (is_selected)
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
            });
        }
        return matched_documents;
    }
};
// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
// ** Добавление документов. Добавленный документ должен находиться по поисковому запросу, который содержит слова из документа.
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        assert(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        assert(doc0.id == doc_id);
    }

// ** Поддержка стоп-слов. Стоп-слова исключаются из текста документов.
    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        assert(server.FindTopDocuments("in"s).empty());
    }
}

/*
Разместите код остальных тестов здесь
*/



// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void Test0() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
// ** Добавление документов. Добавленный документ должен находиться по поисковому запросу, который содержит слова из документа.
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        assert(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        assert(doc0.id == doc_id);
    }
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void Test1() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

// ** Поддержка стоп-слов. Стоп-слова исключаются из текста документов.
    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        assert(server.FindTopDocuments("in"s).empty());
    }
}


// ** Поддержка минус-слов. Документы, содержащие минус-слова поискового запроса, не должны включаться в результаты поиска.
// ** 2
void Test2() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        assert(server.FindTopDocuments("cat"s).empty() == false);
        assert(server.FindTopDocuments("cat -city"s).empty() == true);
    }
}


// ** Матчинг документов. При матчинге документа по поисковому запросу должны быть возвращены все слова из поискового запроса, 
// присутствующие в документе. Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
// ** 3
void Test3_1() {
    const int doc_id = 42;
    const string content = "cat in the city play match"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        assert(get<0>(server.MatchDocument("city cat cian"s, doc_id)).size() == 2);
        //assert(get<0>(server.MatchDocument("cat -city"s, doc_id)).size() == 0);
    }
}

void Test3_2() {
    const int doc_id = 42;
    const string content = "cat in the city play match"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        //assert(get<0>(server.MatchDocument("city cat cian"s, doc_id)).size() == 2);
        assert(get<0>(server.MatchDocument("cat -city"s, doc_id)).size() == 0);
    }
}


// ** Фильтрация результатов поиска с использованием предиката, задаваемого пользователем.
// ** 4
void Test4_1() {
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(1, "cat in the city play match"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(2, "cat1 in the city play match"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(3, "cat1 in the city2 play match"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(4, "cat1 in the city2 play3 match"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(5, "cat1 in the city2 play3 match4"s, DocumentStatus::ACTUAL, ratings);

        auto result = server.FindTopDocuments("cat city play match"s);
        assert(result.empty() == false);
     }
}
// ** Вычисление рейтинга документов. Рейтинг добавленного документа равен среднему арифметическому оценок документа.
void Test4_2() {
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(1, "cat in the city play match"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(2, "cat1 in the city play match"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(3, "cat1 in the city2 play match"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(4, "cat1 in the city2 play3 match"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(5, "cat1 in the city2 play3 match4"s, DocumentStatus::ACTUAL, ratings);

        auto result = server.FindTopDocuments("cat city play match"s);

        auto rating = result.front().rating;
        assert(rating == 2);
     }
}
// ** Сортировка найденных документов по релевантности. Возвращаемые при поиске документов результаты должны быть отсортированы в порядке убывания релевантности.
void Test4_3() {
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(1, "cat in the city play match"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(2, "cat1 in the city play match"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(3, "cat1 in the city2 play match"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(4, "cat1 in the city2 play3 match"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(5, "cat1 in the city2 play3 match4"s, DocumentStatus::ACTUAL, ratings);

        auto result = server.FindTopDocuments("cat city play match"s);
        auto relevance = result.front().relevance;
        for(auto doc:result)
        {
            assert(doc.relevance <= relevance);
            relevance = doc.relevance;
        }
        result = server.FindTopDocuments("cat city play match"s, []([[maybe_unused]] int document_id, [[maybe_unused]]  DocumentStatus status, [[maybe_unused]] int rating){
            return (document_id == 2);
        });
        assert(result.front().id == 2);

     }
}
void Test4_4() {
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(1, "cat in the city play match"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(2, "cat1 in the city play match"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(3, "cat1 in the city2 play match"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(4, "cat1 in the city2 play3 match"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(5, "cat1 in the city2 play3 match4"s, DocumentStatus::ACTUAL, ratings);

        auto result = server.FindTopDocuments("cat city play match"s);
        result = server.FindTopDocuments("cat city play match"s, []([[maybe_unused]] int document_id, [[maybe_unused]]  DocumentStatus status, [[maybe_unused]] int rating){
            return (document_id == 2);
        });
        assert(result.front().id == 2);
     }
}

// ** Поиск документов, имеющих заданный статус.
// ** 5
void Test5() {
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(1, "cat in the city play match"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(2, "cat1 in the city play match"s, DocumentStatus::BANNED, ratings);
        server.AddDocument(3, "cat1 in the city2 play match"s, DocumentStatus::IRRELEVANT, ratings);
        server.AddDocument(4, "cat1 in the city2 play3 match"s, DocumentStatus::REMOVED, ratings);
        server.AddDocument(5, "cat1 in the city2 play3 match4"s, DocumentStatus::ACTUAL, ratings);

        auto result = server.FindTopDocuments("cat city play match"s, DocumentStatus::REMOVED);
        assert(result.front().id == 4);
        
     }
}
// ** Корректное вычисление релевантности найденных документов.
void Test6() {
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(1, "cat in the city play match"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(2, "cat1 in the city play match"s, DocumentStatus::BANNED, ratings);
        server.AddDocument(3, "cat1 in the city2 play match"s, DocumentStatus::IRRELEVANT, ratings);
        server.AddDocument(4, "cat1 in the city2 play3 match"s, DocumentStatus::REMOVED, ratings);
        server.AddDocument(5, "cat1 in the city2 play3 match4"s, DocumentStatus::ACTUAL, ratings);

        auto result = server.FindTopDocuments("cat city play match"s);
        //assert(result.empty() == false);
        assert(abs(result.front().relevance - 0.81492445484711395) < 1e-6);

        // auto rating = result.front().rating;
        // assert(rating == 2);
        // result = server.FindTopDocuments("cat city play match"s, DocumentStatus::REMOVED);
        // assert(result.front().id == 4);
        // assert(abs(result.front().relevance - 0.055785887828552441) < < 1e-6);
        
     }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    Test0();
    Test1();
    Test2();
    Test3_1();
    Test3_2();
    Test4_1();
    Test4_2();
    Test4_3();
    Test4_4();
    Test5();
    Test6();
    // Не забудьте вызывать остальные тесты здесь
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}