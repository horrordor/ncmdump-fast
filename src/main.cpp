#include "main.h"

int main(int argc, char* argv[]) {
#if _OPENMP
	std::cout << "未启动自动并行！" << std::endl;
#else
	std::cout << "已启动自动并行！" << std::endl;
#endif

	std::cout << "请输入网易云音乐下载目录，或者直接按下回车键（默认为C:/CloudMusic）" << std::endl;
	char buffer[100] = { 0 };
	std::cin.get(buffer, 100);
	std::string input = buffer;
	input = input == "" ? "C:/CloudMusic" : input;
	std::cout << "您选择的目录为：" << input << std::endl;

	std::filesystem::path path(input);
	if (!exists(path)) {
		std::cout << "路径不存在！" << input << std::endl;
		return 1;
	}
	else {
		std::filesystem::directory_entry entry(path);
		if (entry.status().type() != std::filesystem::file_type::directory) {
			std::cout << "该路径是不一个目录！" << std::endl;
			return 2;
		}
		else {
			std::filesystem::directory_iterator list(path);
			visite_dir(list);
		}
	}

	std::system("pause");
	return 0;
}