#ifndef NEDITOR

#include "Editor.hpp"
#include <fstream>
#include <filesystem>
#include <vector>
#include "FontAwesome.hpp"

#include "../Logger.hpp"
#include "../Window.hpp"
#include "../AssetManager.hpp"
#include "../CLHelper.hpp"
#include "../Random.hpp"

namespace Editor::ResourcesWindow
{
	class FileRecord
	{
	public:
		std::string path;
		std::string name;
		std::string extension;
		void* texture = nullptr;
		FileRecord(std::string _path, std::string _name, std::string _extension)
			: path(std::move(_path)),
			name(std::move(_name)),
			extension(std::move(_extension)) {}
	};

	class FolderTree
	{
	public:
		FolderTree(std::filesystem::path _path, std::string _name) : path(_path), name(std::move(_name)) {}
	public:
		std::filesystem::path  path;
		FolderTree* parent;
		std::string name;
		std::vector<FolderTree*> folders; // < -- subfolders
		std::vector<FileRecord*> files;
	};

	constexpr uint CppHash  = Random::PathToHash(".cpp");
	constexpr uint HlslHash = Random::PathToHash(".hlsl");
	constexpr uint HppHash  = Random::PathToHash(".hpp");
	constexpr uint MatHash  = Random::PathToHash(".mat");

	constexpr uint blendHash = Random::PathToHash(".blend");
	constexpr uint fbxHash   = Random::PathToHash(".fbx");
	constexpr uint objHash   = Random::PathToHash(".obj");
	constexpr uint cmeshHash = Random::PathToHash(".cmesh");

	std::filesystem::path currentPath;

	void* fileIcon, * folderIcon,
		* meshIcon, * cppIcon,
		* hlslIcon, * hppIcon, * materialIcon;

	FolderTree* rootTree;
	FolderTree* currentTree;

	std::vector<FolderTree*> searchFolders;
	std::vector<FileRecord*> searchFiles;

	const char* GetCurrentPath() { return currentPath.string().c_str(); }

	inline void* ExtensionToIcon(std::string& extension)
	{
		switch (Random::PathToHash(extension.c_str())) // StringToHash Function is located in Helper.hpp
		{
		case CppHash:  extension = "CPP";  return cppIcon;
		case HlslHash: extension = "HLSL"; return hlslIcon;
		case HppHash:  extension = "HPP";  return hppIcon;
		case MatHash:  extension = "MAT";  return materialIcon;
			// is mesh ?
		case fbxHash:
		case objHash:
		case blendHash:
		case cmeshHash:
		{
			extension = "MESH";
			return meshIcon;
		}
		default: fileIcon;
		}
		return fileIcon;
	}

	FolderTree* CreateTreeRec(FolderTree* parent, const std::filesystem::path& path)
	{
		std::string folderName = path.filename().string();
		FolderTree* tree = new FolderTree(path, folderName);
		for (auto& directory : std::filesystem::directory_iterator(path))
		{
			if (directory.is_directory()) continue;
			std::string fileName  = directory.path().filename().string();
			std::string filePath  = directory.path().string();
			std::string extension = directory.path().extension().string();
			void* icon = ExtensionToIcon(extension);

			if (extension == ".png" or extension == ".jpg") {
				const uint hash = Random::PathToHash(filePath.c_str());
				// Texture* texture = new Texture();
				// if (AssetManager::TryGetTexture(texture, filePath, true)) {
				// 	// if texture exist use that
				// 	icon = texture->resourceView;
				// }
				extension = "TEXTURE";
			}

			FileRecord* record = new FileRecord(filePath, fileName, extension);
			record->texture = icon;
			tree->files.push_back(record);
		}
		for (auto& directory : std::filesystem::directory_iterator(path))
		{
			if (!directory.is_directory()) continue;
			tree->folders.push_back(CreateTreeRec(tree, directory.path()));
		}
		tree->parent = parent;
		return tree;
	}

	void Initialize()
	{
		fileIcon = folderIcon = meshIcon = cppIcon = hlslIcon = hppIcon = materialIcon = fileIcon;

		// fileIcon = Texture("Textures/Icons/file.png").resourceView;
		// folderIcon = Texture("Textures/Icons/folder.png").resourceView;
		// meshIcon = Texture("Textures/Icons/mesh.png").resourceView;
		// 
		// cppIcon = Texture("Textures/Icons/cpp_icon.png").resourceView;
		// hlslIcon = Texture("Textures/Icons/hlsl_file_icon.png").resourceView;
		// hppIcon = Texture("Textures/Icons/hpp_icon.png").resourceView;
		// materialIcon = Texture("Textures/Icons/Material_Icon.png").resourceView;

		currentPath = std::filesystem::current_path();

		rootTree = CreateTreeRec(nullptr, currentPath);
		currentTree = rootTree;
	}

	void TreeDrawRec(FolderTree* tree, int& id)
	{
		static auto flags = ImGuiTreeNodeFlags_OpenOnArrow;//: ImGuiTreeNodeFlags_OpenOnArrow;

		ImGui::PushID(id++);
		if (ImGui::TreeNodeEx(tree->name.c_str(), flags))
		{
			for (auto& folder : tree->folders)
			{
				TreeDrawRec(folder, id);
			}
			ImGui::TreePop();
		}
		if (ImGui::IsItemClicked()) { currentTree = tree; }
		ImGui::PopID();
	}

	void TreeWindowDraw(int& id)
	{
		static bool Open = true;

		ImGui::Begin("ResourcesTree", &Open, ImGuiWindowFlags_NoTitleBar);

		TreeDrawRec(rootTree, id);

		ImGui::End();
	}

	void DrawFolders(std::vector<FolderTree*>& folders, int& id)
	{
		FolderTree* folderRec = currentTree;
		// todo scroll and zoom in and out
		for (auto& folder : currentTree->folders)
		{
			ImGui::PushID(id++);

			// if (GUI::ImageButton(folderIcon, Editor::filesize))
			// {
			// 	AXLOG("clicked %s", folder->name);
			// 	currentPath = folder->path;
			// 	folderRec = folder;
			// }
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::Text(folder->name.c_str());
				ImGui::EndTooltip();
			}

			ImGui::TextWrapped("%s", folder->name.c_str());

			ImGui::PopID();
			ImGui::NextColumn();
		}

		currentTree = folderRec;
	}

	void DrawFiles(const std::vector<FileRecord*>& files, int& id)
	{
		for (int i = 0; i < currentTree->files.size(); ++i)
		{
			auto& file = currentTree->files[i];
			ImGui::PushID(id++);

			void* icon = file->texture;

			// GUI::ImageButton(file->texture, Editor::filesize, { 0, 0 }, { 1, 1 });

			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
			{
				// ShellExecute(0, 0, std::wstring(file->path.begin(), file->path.end()).c_str(), 0, 0, SW_SHOW);
			}

			GUI::DragUIElementString(file->path.c_str(), file->extension.c_str(), file->texture);

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::Text(file->name.c_str());
				ImGui::EndTooltip();
			}

			ImGui::TextWrapped("%s", file->name.c_str());

			ImGui::PopID();
			ImGui::NextColumn();
		}
	}


	void RecursiveSearch(const char* key, const int len, FolderTree* tree)
	{
		for (auto& folder : tree->folders)
		{
			for (int i = 0; i + len <= folder->name.size(); ++i)
			{
				for (int j = 0; j < len; ++j)
					if (tolower(key[j]) != tolower(folder->name[i + j]))
						goto next_index_folder;
				searchFolders.push_back(folder);
				break;
			next_index_folder: {}
			}
			RecursiveSearch(key, len, folder);
		}

		for (auto& file : tree->files)
		{
			for (int i = 0; i + len <= file->name.size(); ++i)
			{
				for (int j = 0; j < len; ++j)
					if (tolower(key[j]) != tolower(file->name[i + j]))
						goto next_index_file;
				searchFiles.push_back(file);
				break;
			next_index_file: {}
			}
		}
	}

	void SearchProcess(const char* SearchText)
	{
		const int len = strlen(SearchText);
		searchFolders.clear();
		searchFiles.clear();
		RecursiveSearch(SearchText, len, rootTree);
	}

	void OpenFileLocationLambda()
	{
		// ShellExecute(0, 0, currentPath.generic_wstring().c_str(), 0, 0, SW_SHOW);
	}

	void DrawWindow()
	{
		int id = 0; // for imgui push id
		static bool searching = false;

		TreeWindowDraw(id);

		ImGui::Begin("Resources");

		Editor::TitleAndAction rightClickOptions = Editor::TitleAndAction("Open File Location", OpenFileLocationLambda);

		GUI::RightClickPopUp("Resource Opinions", &rightClickOptions, 1);

		if (GUI::IconButton(ICON_FA_ARROW_LEFT) && currentTree->parent) {
			currentPath = currentPath.parent_path();
			currentTree = currentTree->parent;
		}
		ImGui::SameLine();
		ImGui::Text(ICON_FA_SEARCH);
		ImGui::SameLine();
		static char SearchText[128];

		if (ImGui::InputText("Search", SearchText, 128))
		{
			SearchProcess(SearchText);
		}

		ImGui::Text((const char*)currentPath.string().c_str());

		searching = strlen(SearchText);

		ImGui::Text(searching ? "searching" : "not searching");

		static float padding = 3.14f;
		static float thumbnailSize = 64.0f;
		float cellSize = 64.0f + padding;
		float panelWidth = ImGui::GetContentRegionAvail().x;
		int columnCount = std::max(1, (int)floor(panelWidth / cellSize));
		ImGui::Columns(columnCount, "resources-columns", false);

		if (!searching) {
			DrawFolders(currentTree->folders, id);
			DrawFiles(currentTree->files, id);
		}
		else
		{
			DrawFolders(searchFolders, id);
			DrawFiles(searchFiles, id);
		}

		ImGui::Columns(1);

		ImGui::End();
	}
}
#endif