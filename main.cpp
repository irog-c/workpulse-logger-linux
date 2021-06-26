#include <iostream>
#include <string>
#include <exception>
#include <fstream>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <locale.h>
#include <pwd.h>

#include <X11/Xlib.h>
#include <X11/Xmu/WinUtil.h>

Bool xerror = False;

Display* open_display()
{
	Display* d = XOpenDisplay(NULL);

	if(d == NULL)
		throw std::runtime_error("Failed to open display.");
	else
		return d;
}

int handle_error(Display* display, XErrorEvent* error)
{
	printf("ERROR: X11 error\n");
	xerror = True;
	return 1;
}

Window get_focus_window(Display* d)
{
	Window w;
	int revert_to;
	XGetInputFocus(d, &w, &revert_to); // see man

	if(xerror)
		throw std::runtime_error("Failed to get focus window.");
	
	if(w == None)
		throw std::runtime_error("No focus window.");

  return w;
}

Window get_top_window(Display* d, Window start)
{
	Window w = start;
	Window parent = start;
	Window root = None;
	Window *children;
	unsigned int nchildren;
	Status s;

	while(parent != root) 
	{
		w = parent;
		s = XQueryTree(d, w, &root, &parent, &children, &nchildren); // see man

		if(s) XFree(children);

		if(xerror)
		{
			throw std::runtime_error("Failed to get top window.");
			exit(1);
		}
	}

	return w;
}

// search a named window (that has a WM_STATE prop)
// on the descendent windows of the argment Window.
Window get_named_window(Display* d, Window start)
{
	Window w = XmuClientWindow(d, start); // see man
	return w;
}

// (XFetchName cannot get a name with multi-byte chars)
[[nodiscard]] std::string get_window_name(Display* d, Window w)
{
	XTextProperty prop;
	Status s;

	s = XGetWMName(d, w, &prop); // see man
	if(!xerror && s)
	{
		int count = 0, result;
		char **list = NULL;
		result = XmbTextPropertyToTextList(d, &prop, &list, &count); // see man
		if(result == Success)
			return std::string(list[0]);
		else
			return std::string("ERROR: XmbTextPropertyToTextList\n");
	}
	else return std::string("ERROR: XGetWMName\n");
}

[[nodiscard]] std::string get_window_class(Display* d, Window w)
{
	Status s;
	XClassHint* cl;

	cl = XAllocClassHint(); // see man
	if(xerror)
		return std::string("ERROR: XAllocClassHint\n");

  	s = XGetClassHint(d, w, cl); // see man
	if(xerror || s) 
		return std::string(cl->res_class);
	else 
		return std::string("ERROR: XGetClassHint\n");
}

struct window_info_t
{
	std::string win_name;
	std::string win_class;
};

window_info_t get_window_info(Display* d, Window w)
{
	w = get_focus_window(d);
	w = get_top_window(d, w);
	w = get_named_window(d, w);

	return { get_window_name(d, w), get_window_class(d, w) };
}

bool get_camera_active()
{
	static constexpr char path[] = "/proc/modules";
 	std::string line;
  	std::ifstream ifs(path);

    while(!ifs.eof()) 
    {
		getline(ifs, line);

		if(line.rfind("uvcvideo", 0) == 0)
		{
			auto sub_line = line.substr(0, line.find(" -"));
			if(sub_line[sub_line.length()-1] != '0')
				return true;
		}
    }

	ifs.close();
	return false;
}

std::string USER = "";

std::string get_username()
{
	uid_t uid = geteuid ();
	struct passwd *pw = getpwuid(uid);
	if (pw)
	{
		return std::string(pw->pw_name);
	}
	return "";
}

void log_app(const window_info_t& info)
{
	std::string path = "/home/" + USER + "/log-app";

	std::ofstream logfile;
	logfile.open(path, std::ios_base::app);
	logfile << "Window name [" << info.win_name << "] window class [" << info.win_class << "]" << '\n';
	logfile.close();
}

void log_camera(const bool active)
{
	std::string path = "/home/" + USER + "/log-camera";

	std::ofstream logfile;
	logfile.open(path, std::ios_base::app);
	if(active)
	{
		logfile << "camera active" << '\n'; 
	}
	else
	{
		logfile << "camera not active" << '\n';
	}

	logfile.close();
}

int main(void) try
{
	Display* d;
	Window w;

	setlocale(LC_ALL, "");

	d = open_display();
	XSetErrorHandler(handle_error);

	USER = get_username();

	window_info_t info = get_window_info(d, w);
	window_info_t last_info = info;
	
	bool camera_active;
	bool last_camera_active = camera_active = false;

	for(;;)
	{
		info = get_window_info(d, w);
		camera_active = get_camera_active();
		
		if(camera_active != last_camera_active)
		{
			log_camera(camera_active);
			last_camera_active = camera_active;
		}

		if(info.win_name != last_info.win_name)
		{
			log_app(info);
			last_info = info;
		}

		usleep(2000000);
	}

	return EXIT_SUCCESS;
}
catch(const std::exception& e)
{
	std::cout << "Exception: " << e.what() << '\n';
}