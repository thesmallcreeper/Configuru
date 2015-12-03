#include <iostream>

#include <boost/filesystem.hpp>

#define LOGURU_IMPLEMENTATION 1
#include "loguru.hpp"

#define CONFIGURU_ASSERT(test) CHECK_F(test)

#define CONFIGURU_IMPLEMENTATION 1
#include "../configuru.hpp"

namespace fs = boost::filesystem;

std::vector<fs::path> list_files(fs::path directory, std::string extension)
{
    std::vector<fs::path> result;
    fs::directory_iterator it(directory);
    fs::directory_iterator end;
    for (; it != end; ++it) {
        if (fs::is_regular_file(it->status()) && fs::extension(*it) == extension) {
            result.push_back(it->path());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

void test_code(std::string test_name, bool should_pass, size_t& num_run, size_t& num_failed, std::function<void()> code)
{
	try {
		code();

		if (should_pass) {
			std::cout << loguru::terminal_green() << "PASS: " << loguru::terminal_reset() << test_name << std::endl;
		} else {
			std::cout <<loguru::terminal_red() <<  "SHOULD NOT HAVE PARSED: " << loguru::terminal_reset() << test_name << std::endl;
			num_failed += 1;
		}
	} catch (std::exception& e) {
		if (should_pass) {
			std::cout << loguru::terminal_red() << "FAILED: " << loguru::terminal_reset() << test_name << ": " << e.what() << std::endl << std::endl;
			num_failed += 1;
		} else {
			std::cout << loguru::terminal_green() << "PASS: " << loguru::terminal_reset() << test_name << ": " << e.what() << std::endl << std::endl;
		}
	}
	num_run += 1;
}

void test_parse(FormatOptions options, bool should_pass, fs::path path, size_t& num_run, size_t& num_failed)
{
	test_code(path.filename().string(), should_pass, num_run, num_failed, [&](){
		configuru::parse_config_file(path.string(), options);
	});
}

void test_all_in(FormatOptions options, bool should_pass, fs::path dir, std::string extension, size_t& num_run, size_t& num_failed)
{
	for (auto path : list_files(dir, extension)) {
		test_parse(options, should_pass, path.string(), num_run, num_failed);
	}
}

void test_special(size_t& num_run, size_t& num_failed)
{
	auto format = configuru::JSON;
	format.enforce_indentation = true;
	format.indentation = "\t";
	test_parse(format, false, "../../test_suite/special/two_spaces_indentation.json", num_run, num_failed);

	format.indentation = "    ";
	test_parse(format, false, "../../test_suite/special/two_spaces_indentation.json", num_run, num_failed);

	format.indentation = "  ";
	test_parse(format, true, "../../test_suite/special/two_spaces_indentation.json", num_run, num_failed);
}

void test_bad_usage(size_t& num_run, size_t& num_failed)
{
	auto config = configuru::parse_config_file("../../test_suite/special/config.json", configuru::JSON);
	test_code("access_bool_as_bool", true, num_run, num_failed, [&]{
		auto b = (bool)config["some_bool"];
		(void)b;
	});
	test_code("access_bool_as_float", false, num_run, num_failed, [&]{
		auto f = (float)config["some_bool"];
		(void)f;
	});
	test_code("key_not_found", false, num_run, num_failed, [&]{
		std::cout << (float)config["does_not_exist"];
	});
}

void run_unit_tests()
{
	size_t num_run = 0;
	size_t num_failed = 0;
	std::cout << "JSON expected to pass:" << std::endl;
	test_all_in(configuru::JSON, true, "../../test_suite/json_pass",      ".json", num_run, num_failed);
	test_all_in(configuru::JSON, true, "../../test_suite/json_only_pass", ".json", num_run, num_failed);

	std::cout << std::endl << "JSON expected to fail:" << std::endl;
	test_all_in(configuru::JSON, false, "../../test_suite/json_fail", ".json", num_run, num_failed);
	test_all_in(configuru::JSON, false, "../../test_suite/cfg_pass",  ".cfg", num_run, num_failed);
	test_all_in(configuru::JSON, false, "../../test_suite/cfg_fail",  ".cfg", num_run, num_failed);

	std::cout << std::endl << "CFG expected to pass:" << std::endl;
	test_all_in(configuru::CFG, true,  "../../test_suite/json_pass", ".json", num_run, num_failed);
	test_all_in(configuru::CFG, true,  "../../test_suite/cfg_pass",  ".cfg", num_run, num_failed);

	std::cout << std::endl << "CFG expected to fail:" << std::endl;
	test_all_in(configuru::CFG, false, "../../test_suite/json_only_pass", ".json", num_run, num_failed);
	test_all_in(configuru::CFG, false, "../../test_suite/json_fail",      ".json", num_run, num_failed);
	test_all_in(configuru::CFG, false, "../../test_suite/cfg_fail",       ".cfg", num_run, num_failed);
	std::cout << std::endl << std::endl;

	test_special(num_run, num_failed);

	test_bad_usage(num_run, num_failed);

	if (num_failed == 0) {
		printf("%s%lu/%lu tests passed!%s\n", loguru::terminal_green(), num_run, num_run, loguru::terminal_reset());
	} else {
		printf("%s%lu/%lu tests failed.%s\n", loguru::terminal_red(), num_failed, num_run, loguru::terminal_reset());
	}
	printf("\n\n");
	fflush(stdout);
}

const char* TEST_CFG = R"(
pi:    3.14,
array: [1 2 3 4]
obj {
	// A comment
	nested_value: 42
}
)";

void parse_and_print()
{
	auto cfg = configuru::parse_config(TEST_CFG, FormatOptions(), "test_cfg");
	std::cout << "pi: " << cfg["pi"] << std::endl;
	cfg.check_dangling();

	std::cout << std::endl;
	std::cout << "// CFG:" << std::endl;
	std::cout << configuru::write_config(cfg, configuru::CFG);

	std::cout << std::endl;
	std::cout << "// JSON with tabs:" << std::endl;
	std::cout << configuru::write_config(cfg, configuru::JSON);

	std::cout << std::endl;
	std::cout << "// JSON with two spaces:" << std::endl;
	auto format = configuru::JSON;
	format.indentation = "  ";
	std::cout << configuru::write_config(cfg, format);

	std::cout << std::endl;
	std::cout << "// JSON with keys sorted lexicographically:" << std::endl;
	format.sort_keys = true;
	std::cout << configuru::write_config(cfg, format);

	std::cout << std::endl;
	std::cout << "// Compact JSON:" << std::endl;
	format = configuru::JSON;
	format.indentation = "";
	std::cout << configuru::write_config(cfg, format);

	std::cout << std::endl;
}

void create()
{
	/*
	Based on https://github.com/nlohmann/json#examples

	Taget JSON:

	{
		"pi":      3.141,
		"happy":   true,
		"name":    "Emil",
		"nothing": null,
		"answer":  {
			"everything": 42
		},
		"array":   [1, 0, 2],
		"object": {
			"currency": "USD",
			"value":    42.99
		}
	}
	*/

	// Create the config as an object:
	auto cfg = Config::object();

	// add a number that is stored as double
	cfg["pi"] = 3.141;

	// add a Boolean that is stored as bool
	cfg["happy"] = true;

	// add a string that is stored as std::string
	cfg["name"] = "Emil";

	// add another null entry by passing nullptr
	cfg["nothing"] = nullptr;

	// add an object inside the object
	cfg["answer"] = Config::object();
	cfg["answer"]["everything"] = 42;

	// add an array that is stored as std::vector (using an initializer_list)
	cfg["array"] = { 1, 0, 2 };

	// add another object (using an initializer_list of pairs)
	cfg["object"] = {
		{ "currency", "USD" },
		{ "value",    42.99 }
	};

	// instead, you could also write (which looks very similar to the JSON above)
	configuru::Config cfg2 = {
		{ "pi",      3.141   },
		{ "happy",   true    },
		{ "name",    "Emil"  },
		{ "nothing", nullptr },
		{ "answer",  {
			{ "everything", 42 }
		} },
		{ "array",   { 1, 0, 2 } },
		{ "object", {
			{ "currency", "USD" },
			{ "value",    42.99 }
		} }
	};

	std::cout << "cfg:\n"  << cfg  << std::endl;
	std::cout << "cfg2:\n" << cfg2 << std::endl;
}

void test_comments()
{
	auto in_path  = "../../test_suite/comments_in.cfg";
	auto out_path = "../../test_suite/comments_out.cfg";
	auto data = parse_config_file(in_path, configuru::CFG);
	write_config_file(out_path, data, configuru::CFG);
}

int main()
{
	run_unit_tests();
	parse_and_print();
	create();
	test_comments();
}