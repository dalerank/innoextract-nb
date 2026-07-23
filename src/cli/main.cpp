/*
 * Copyright (C) 2011-2020 Daniel Scharrer
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the author(s) be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "release.hpp"

#include "cli/extract.hpp"

#include "setup/version.hpp"

#include "util/boostfs_compat.hpp"
#include "util/console.hpp"
#include "util/fstream.hpp"
#include "util/log.hpp"
#include "util/time.hpp"
#include "util/windows.hpp"

namespace fs = std::filesystem;

enum ExitValues {
	ExitSuccess = 0,
	ExitUserError = 1,
	ExitDataError = 2
};

namespace {

//! Locale-independent ASCII case-insensitive comparison, equivalent to boost::iequals()
//! for the (ASCII-only) values handled here.
bool iequals(const std::string & a, const std::string & b) {
	if(a.size() != b.size()) {
		return false;
	}
	for(size_t i = 0; i < a.size(); i++) {
		char ca = a[i], cb = b[i];
		if(ca >= 'A' && ca <= 'Z') { ca = char(ca - 'A' + 'a'); }
		if(cb >= 'A' && cb <= 'Z') { cb = char(cb - 'A' + 'a'); }
		if(ca != cb) {
			return false;
		}
	}
	return true;
}

//! Error thrown by \ref option_parser on invalid command-lines.
struct cli_error : public std::runtime_error {
	explicit cli_error(const std::string & msg) : std::runtime_error(msg) { }
};

/*!
 * Minimal hand-rolled replacement for boost::program_options, supporting just the
 * subset of functionality (long/short flags, "=value" or space-separated values,
 * boolean flags with an implicit value, repeatable options and positional arguments)
 * that innoextract's command-line actually uses.
 */
class option_parser {
	
	struct spec {
		std::string long_name;
		char short_name;
		bool has_value;
		bool is_bool;  // value is optional and defaults to "true" if omitted
		bool multi;    // option can be repeated, values are accumulated
	};
	
	std::vector<spec> specs;
	std::map<std::string, size_t> by_long;
	std::map<char, size_t> by_short;
	
	std::map<std::string, size_t> counts;
	std::map<std::string, std::string> values;
	std::map<std::string, std::vector<std::string> > multi_values;
	std::vector<std::string> positional_args;
	
	void set_value(const spec & s, const std::string & value) {
		if(s.multi) {
			multi_values[s.long_name].push_back(value);
		} else {
			values[s.long_name] = value;
		}
		counts[s.long_name]++;
	}
	
	void set_flag(const spec & s) {
		counts[s.long_name]++;
	}
	
public:
	
	void add(const std::string & long_name, char short_name = 0, bool has_value = false,
	         bool is_bool = false, bool multi = false) {
		spec s;
		s.long_name = long_name, s.short_name = short_name;
		s.has_value = has_value, s.is_bool = is_bool, s.multi = multi;
		specs.push_back(s);
		by_long[long_name] = specs.size() - 1;
		if(short_name) {
			by_short[short_name] = specs.size() - 1;
		}
	}
	
	void parse(int argc, char * const argv[]) {
		
		bool options_ended = false;
		
		for(int i = 1; i < argc; i++) {
			
			std::string arg = argv[i];
			
			if(!options_ended && arg == "--") {
				options_ended = true;
				continue;
			}
			
			if(!options_ended && arg.size() >= 2 && arg[0] == '-' && arg[1] == '-') {
				
				// Long option: --name or --name=value
				std::string name = arg.substr(2);
				std::string value;
				bool has_explicit_value = false;
				size_t eq = name.find('=');
				if(eq != std::string::npos) {
					value = name.substr(eq + 1);
					name = name.substr(0, eq);
					has_explicit_value = true;
				}
				
				std::map<std::string, size_t>::const_iterator it = by_long.find(name);
				if(it == by_long.end()) {
					throw cli_error("unrecognised option '--" + name + "'");
				}
				const spec & s = specs[it->second];
				
				if(!s.has_value) {
					if(has_explicit_value) {
						throw cli_error("option '--" + name + "' does not take a value");
					}
					set_flag(s);
				} else if(s.is_bool) {
					set_value(s, has_explicit_value ? value : "true");
				} else {
					if(!has_explicit_value) {
						if(i + 1 >= argc) {
							throw cli_error("option '--" + name + "' requires a value");
						}
						value = argv[++i];
					}
					set_value(s, value);
				}
				
				continue;
			}
			
			if(!options_ended && arg.size() >= 2 && arg[0] == '-') {
				
				// One or more short options, possibly grouped: -e, -el, -dvalue, -d value
				size_t pos = 1;
				while(pos < arg.size()) {
					
					char c = arg[pos];
					std::map<char, size_t>::const_iterator it = by_short.find(c);
					if(it == by_short.end()) {
						throw cli_error(std::string("unrecognised option '-") + c + "'");
					}
					const spec & s = specs[it->second];
					
					if(!s.has_value) {
						set_flag(s);
						pos++;
						continue;
					}
					
					// The rest of this token (if any) is the option's value.
					std::string rest = arg.substr(pos + 1);
					if(!rest.empty() && rest[0] == '=') {
						rest = rest.substr(1);
					}
					
					if(!rest.empty()) {
						set_value(s, rest);
					} else if(s.is_bool) {
						set_value(s, "true");
					} else {
						if(i + 1 >= argc) {
							throw cli_error(std::string("option '-") + c + "' requires a value");
						}
						set_value(s, argv[++i]);
					}
					
					break; // the remainder of the token was consumed as the value
					
				}
				
				continue;
			}
			
			positional_args.push_back(arg);
			
		}
		
	}
	
	bool count(const std::string & name) const {
		return counts.count(name) != 0;
	}
	
	bool has(const std::string & name) const {
		return values.count(name) != 0;
	}
	
	const std::string & value(const std::string & name) const {
		return values.at(name);
	}
	
	const std::vector<std::string> & values_of(const std::string & name) const {
		static const std::vector<std::string> empty_result;
		std::map<std::string, std::vector<std::string> >::const_iterator it = multi_values.find(name);
		return (it != multi_values.end()) ? it->second : empty_result;
	}
	
	const std::vector<std::string> & positional() const {
		return positional_args;
	}
	
};

//! Parse an unsigned 32-bit integer command-line argument.
std::uint32_t parse_uint32(const std::string & s) {
	if(s.empty()) {
		throw cli_error("expected a number, got \"\"");
	}
	size_t pos = 0;
	unsigned long value;
	try {
		value = std::stoul(s, &pos);
	} catch(...) {
		throw cli_error("expected a number, got \"" + s + '"');
	}
	if(pos != s.size() || value > std::numeric_limits<std::uint32_t>::max()) {
		throw cli_error("expected a number, got \"" + s + '"');
	}
	return std::uint32_t(value);
}

struct option_info {
	const char * args;
	const char * description;
};

void print_option_group(std::ostream & os, const char * title, const option_info * options, size_t count) {
	os << '\n' << title << ":\n";
	for(size_t i = 0; i < count; i++) {
		os << "  " << std::left << std::setw(28) << options[i].args << options[i].description << '\n';
	}
}

} // anonymous namespace

static const char * get_command(const char * argv0) {
	
	if(!argv0) {
		argv0 = innoextract_name;
	}
	std::string var = argv0;
	
#ifdef _WIN32
	size_t pos = var.find_last_of("/\\");
#else
	size_t pos = var.find_last_of('/');
#endif
	if(pos != std::string::npos) {
		var = var.substr(pos + 1);
	}
	
	var += "_COMMAND";
	
	const char * env = std::getenv(var.c_str());
	if(env) {
		return env;
	} else {
		return argv0;
	}
}

static void print_version(const extract_options & o) {
	if(o.silent) {
		std::cout << innoextract_version << '\n';
		return;
	}
	std::cout << color::white << innoextract_name
	          << ' ' << innoextract_version << color::reset
#ifdef DEBUG
	          << " (with debug output)"
#endif
	          << '\n';
	if(!o.quiet) {
		std::cout << "Extracts installers created by " << color::cyan
		          << innosetup_versions << color::reset << '\n';
	}
}

static void print_help(const char * name) {
	
	std::cout << color::white << "Usage: " << name << " [options] <setup file(s)>\n\n"
	          << color::reset;
	std::cout << "Extract files from an Inno Setup installer.\n";
	std::cout << "For multi-part installers only specify the exe file.\n";
	
	static const option_info generic[] = {
		{ "-h [ --help ]",          "Show supported options" },
		{ "-v [ --version ]",       "Print version information" },
		{ "--license",              "Show license information" },
	};
	print_option_group(std::cout, "Generic options", generic, std::size(generic));
	
	static const option_info action[] = {
		{ "-t [ --test ]",          "Only verify checksums, don't write anything" },
		{ "-e [ --extract ]",       "Extract files (default action)" },
		{ "-l [ --list ]",          "Only list files, don't write anything" },
		{ "--list-sizes",           "List file sizes" },
		{ "--list-checksums",       "List file checksums" },
		{ "-i [ --info ]",          "Print information about the installer" },
		{ "--list-languages",       "List languages supported by the installer" },
		{ "--gog-game-id",          "Determine the installer's GOG.com game ID" },
		{ "--show-password",       "Show password check information" },
		{ "--check-password",      "Abort if the password is incorrect" },
		{ "-V [ --data-version ]",  "Only print the data version" },
#ifdef DEBUG
		{ "--dump-headers",         "Dump decompressed setup headers" },
#endif
	};
	print_option_group(std::cout, "Actions", action, std::size(action));
	
	static const option_info modifiers[] = {
		{ "--codepage arg",             "Encoding for ANSI strings" },
		{ "--collisions arg",           "How to handle duplicate files" },
		{ "--default-language arg",     "Default language for renaming" },
		{ "--dump",                     "Dump contents without converting filenames" },
		{ "-L [ --lowercase ]",         "Convert extracted filenames to lower-case" },
		{ "-T [ --timestamps ] arg",    "Timezone for file times or \"local\" or \"none\"" },
		{ "-d [ --output-dir ] arg",    "Extract files into the given directory" },
		{ "-P [ --password ] arg",      "Password for encrypted files" },
		{ "--password-file arg",        "File to load password from" },
		{ "-g [ --gog ]",               "Extract additional archives from GOG.com installers" },
		{ "--no-gog-galaxy",            "Don't re-assemble GOG Galaxy file parts" },
		{ "-n [ --no-extract-unknown ]", "Don't extract unknown Inno Setup versions" },
	};
	print_option_group(std::cout, "Modifiers", modifiers, std::size(modifiers));
	
	static const option_info filter[] = {
		{ "-m [ --exclude-temp ]",  "Don't extract temporary files" },
		{ "--language arg",         "Extract only files for this language" },
		{ "--language-only",        "Only extract language-specific files" },
		{ "-I [ --include ] arg",   "Extract only files that match this path" },
	};
	print_option_group(std::cout, "Filters", filter, std::size(filter));
	
	static const option_info display[] = {
		{ "-q [ --quiet ]",         "Output less information" },
		{ "-s [ --silent ]",        "Output only error/warning information" },
		{ "--no-warn-unused",       "Don't warn on unused .bin files" },
		{ "-c [ --color ] arg",     "Enable/disable color output" },
		{ "-p [ --progress ] arg",  "Enable/disable the progress bar" },
		{ "  --progress-machine",   "Emit PROGRESS <0..1> lines on stderr for UIs" },
#ifdef DEBUG
		{ "--debug",                "Output debug information" },
#endif
	};
	print_option_group(std::cout, "Display options", display, std::size(display));
	
	std::cout << '\n';
	std::cout << "Extracts installers created by " << color::cyan
	          << innosetup_versions << color::reset << '\n';
	std::cout << '\n';
	std::cout << color::white << innoextract_name
	          << ' ' << innoextract_version << color::reset
	          << ' ' << innoextract_copyright << '\n';
	std::cout << "This is free software with absolutely no warranty.\n";
}

static void print_license() {
	
	std::cout << color::white << innoextract_name
	          << ' ' << innoextract_version << color::reset
	          << ' ' << innoextract_copyright << '\n';
	std::cout << '\n'<< innoextract_license << '\n';
	;
}

int main(int argc, char * argv[]) {
	
	option_parser parser;
	
	// Generic options
	parser.add("help", 'h');
	parser.add("version", 'v');
	parser.add("license");
	
	// Actions
	parser.add("test", 't');
	parser.add("extract", 'e');
	parser.add("list", 'l');
	parser.add("list-sizes");
	parser.add("list-checksums");
	parser.add("info", 'i');
	parser.add("list-languages");
	parser.add("gog-game-id");
	parser.add("show-password");
	parser.add("check-password");
	parser.add("data-version", 'V');
#ifdef DEBUG
	parser.add("dump-headers");
#endif
	
	// Modifiers
	parser.add("codepage", 0, true);
	parser.add("collisions", 0, true);
	parser.add("default-language", 0, true);
	parser.add("dump");
	parser.add("lowercase", 'L');
	parser.add("timestamps", 'T', true);
	parser.add("output-dir", 'd', true);
	parser.add("password", 'P', true);
	parser.add("password-file", 0, true);
	parser.add("gog", 'g');
	parser.add("no-gog-galaxy");
	parser.add("no-extract-unknown", 'n');
	
	// Filters
	parser.add("exclude-temp", 'm');
	parser.add("language", 0, true);
	parser.add("language-only");
	parser.add("include", 'I', true, false, true);
	
	// Display options
	parser.add("quiet", 'q');
	parser.add("silent", 's');
	parser.add("no-warn-unused");
	parser.add("color", 'c', true, true);
	parser.add("progress", 'p', true, true);
	parser.add("progress-machine");
#ifdef DEBUG
	parser.add("debug");
#endif
	
	// Parse the command-line.
	try {
		parser.parse(argc, argv);
	} catch(std::exception & e) {
		color::init(color::disable, color::disable); // Be conservative
		std::cerr << "Error parsing command-line: " << e.what() << "\n\n";
		print_help(get_command(argv[0]));
		return ExitUserError;
	}
	
	::extract_options o;
	
	// Verbosity settings.
	o.silent = parser.count("silent");
	o.quiet = o.silent || parser.count("quiet");
	logger::quiet = o.quiet;
#ifdef DEBUG
	if(parser.count("debug")) {
		logger::debug = true;
	}
#endif
	
	o.warn_unused = !parser.count("no-warn-unused");
	
	// Color / progress bar settings.
	color::is_enabled color_e;
	if(!parser.has("color")) {
		color_e = o.silent ? color::disable : color::automatic;
	} else {
		color_e = iequals(parser.value("color"), "true") ? color::enable : color::disable;
	}
	color::is_enabled progress_e;
	if(!parser.has("progress")) {
		progress_e = o.silent ? color::disable : color::automatic;
	} else {
		progress_e = iequals(parser.value("progress"), "true") ? color::enable : color::disable;
	}
	color::init(color_e, progress_e);
	if(parser.count("progress-machine")) {
		progress::set_machine_progress(true);
	}
	
	// Help output.
	if(parser.count("help")) {
		print_help(get_command(argv[0]));
		return ExitSuccess;
	}
	
	// License output
	if(parser.count("license")) {
		print_license();
		return ExitSuccess;
	}
	
	// Main action.
	o.list_sizes = parser.count("list-sizes");
	o.list_checksums = parser.count("list-checksums");
	bool explicit_list = parser.count("list");
	o.list = explicit_list || o.list_sizes || o.list_checksums;
	o.extract = parser.count("extract");
	o.test = parser.count("test");
	o.list_languages = parser.count("list-languages");
	o.gog_game_id = parser.count("gog-game-id");
	o.show_password = parser.count("show-password");
	o.check_password = parser.count("check-password");
	if(parser.count("info")) {
		o.list_languages = true;
		o.gog_game_id = true;
		o.show_password = true;
	}
	bool explicit_action = o.list || o.test || o.extract || o.list_languages
	                       || o.gog_game_id || o.show_password || o.check_password;
	if(!explicit_action) {
		o.extract = true;
	}
	if(!o.extract && !o.test) {
		progress::set_enabled(false);
	}
	if(!o.silent && (o.test || o.extract)) {
		o.list = true;
	}
	if(!o.quiet && explicit_list) {
		o.list_sizes = true;
	}
	
	// Additional actions.
	o.filenames.set_expand(!parser.count("dump"));
	o.filenames.set_lowercase(parser.count("lowercase"));
	
	// File timestamps
	{
		o.preserve_file_times = true, o.local_timestamps = false;
		if(parser.has("timestamps")) {
			const std::string & timezone_name = parser.value("timestamps");
			if(iequals(timezone_name, "none")) {
				o.preserve_file_times = false;
			} else if(!iequals(timezone_name, "UTC")) {
				o.local_timestamps = true;
				if(!iequals(timezone_name, "local")) {
					util::set_local_timezone(timezone_name);
				}
			}
		}
	}
	
	// List version.
	if(parser.count("version")) {
		print_version(o);
		if(!explicit_action) {
			return ExitSuccess;
		}
	}
	
	o.codepage = parser.has("codepage") ? parse_uint32(parser.value("codepage")) : 0;
	
	{
		o.collisions = OverwriteCollisions;
		if(parser.has("collisions")) {
			const std::string & collisions = parser.value("collisions");
			if(collisions == "overwrite")  {
				o.collisions = OverwriteCollisions;
			} else if(collisions == "rename") {
				o.collisions = RenameCollisions;
			} else if(collisions == "rename-all") {
				o.collisions = RenameAllCollisions;
			} else if(collisions == "error") {
				o.collisions = ErrorOnCollisions;
			} else {
				log_error << "Unsupported --collisions value: " << collisions;
				return ExitUserError;
			}
		}
	}
	if(parser.has("default-language")) {
		o.default_language = parser.value("default-language");
	}
	
	o.extract_temp = !parser.count("exclude-temp");
	if(parser.has("language")) {
		o.language = parser.value("language");
	}
	o.language_only = parser.count("language-only");
	if(parser.has("include")) {
		o.include = parser.values_of("include");
	}
	
	if(parser.positional().empty()) {
		if(!o.silent) {
			std::cout << get_command(argv[0]) << ": no input files specified\n";
			std::cout << "Try the --help (-h) option for usage information.\n";
		}
		return ExitSuccess;
	}
	
	if(parser.has("output-dir")) {
		/*
		 * On Windows, util::u8path() must be used to interpret the given string as
		 * UTF-8, as std::filesystem::path's implicit conversion from std::string
		 * would otherwise use the current codepage.
		 */
		o.output_dir = util::u8path(parser.value("output-dir"));
	}
	
	{
		bool have_password = parser.has("password");
		bool have_password_file = parser.has("password-file");
		if(have_password && have_password_file) {
			log_error << "Combining --password and --password-file is not allowed";
			return ExitUserError;
		}
		if(have_password) {
			o.password = parser.value("password");
		}
		if(have_password_file) {
			std::istream * is = &std::cin;
			fs::path file = util::u8path(parser.value("password-file"));
			util::ifstream ifs;
			if(util::as_string(file) != "-") {
				ifs.open(file);
				if(!ifs.is_open()) {
					log_error << "Could not open password file " << util::as_string(file);
					return ExitDataError;
				}
				is = &ifs;
			}
			std::getline(*is, o.password);
			if(!o.password.empty() && o.password[o.password.size() - 1] == '\n') {
				o.password.resize(o.password.size() - 1);
			}
			if(!o.password.empty() && o.password[o.password.size() - 1] == '\r') {
				o.password.resize(o.password.size() - 1);
			}
			if(!*is) {
				log_error << "Could not read password file " << util::as_string(file);
				return ExitDataError;
			}
		}
		if(o.check_password && o.password.empty()) {
			log_error << "Combining --check-password requires a password";
			return ExitUserError;
		}
	}
	
	o.gog = parser.count("gog");
	o.gog_galaxy = !parser.count("no-gog-galaxy");
	
	o.data_version = parser.count("data-version");
	if(o.data_version) {
		logger::quiet = true;
		if(explicit_action) {
			log_error << "Combining --data-version with other options is not allowed";
			return ExitUserError;
		}
	}
	
#ifdef DEBUG
	o.dump_headers = parser.count("dump-headers");
	if(o.dump_headers) {
		if(explicit_action || o.data_version) {
			log_error << "Combining --dump-headers with other options is not allowed";
			return ExitUserError;
		}
	}
#endif
	
	o.extract_unknown = !parser.count("no-extract-unknown");
	
	const std::vector<std::string> & files = parser.positional();
	
	bool suggest_bug_report = false;
	try {
		for(const std::string & file : files) {
			process_file(util::u8path(file), o);
			if(!o.data_version && files.size() > 1) {
				std::cout << '\n';
			}
		}
	} catch(const std::ios_base::failure & e) {
		log_error << "Stream error while extracting files!\n"
		          << " └─ error reason: " << e.what();
		suggest_bug_report = true;
	} catch(const format_error & e) {
		log_error << e.what();
		suggest_bug_report = true;
	} catch(const std::runtime_error & e) {
		log_error << e.what();
	} catch(const setup::version_error &) {
		log_error << "Not a supported Inno Setup installer!";
	}
	
	if(suggest_bug_report) {
		std::cerr << color::blue << "If you are sure the setup file is not corrupted,"
		          << " consider \nfiling a bug report at "
		          << color::dim_cyan << innoextract_bugs << color::reset << '\n';
	}
	
	if(!logger::quiet || logger::total_errors || logger::total_warnings) {
		progress::clear();
		std::ostream & os = logger::quiet ? std::cerr : std::cout;
		os << color::green << "Done" << color::reset << std::dec;
		if(logger::total_errors || logger::total_warnings) {
			os << " with ";
			if(logger::total_errors) {
				os << color::red << logger::total_errors
				   << ((logger::total_errors == 1) ? " error" : " errors")
				   << color::reset;
			}
			if(logger::total_errors && logger::total_warnings) {
				os << " and ";
			}
			if(logger::total_warnings) {
				os << color::yellow << logger::total_warnings
				   << ((logger::total_warnings == 1) ? " warning" : " warnings")
				   << color::reset;
			}
		}
		os << '.' << std::endl;
	}
	
	return logger::total_errors == 0 ? ExitSuccess : ExitDataError;
}
