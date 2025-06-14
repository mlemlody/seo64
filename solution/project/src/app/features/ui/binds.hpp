#pragma once

#include "../../../game/game.hpp"

enum class BindType
{
	TOGGLE, HOLD
};

struct Bind final
{
public:
	std::string name{};
	int32_t key{};
	BindType type{};
	std::any val_off{};
	std::any val_on{};
	ConfigVar *var{};
	std::string cfg_var_name{};
	std::string fmt{}; //ints & floats
	std::vector<std::pair<std::string, int>> selects{}; //selectsingle
	bool appear_in_bindslist{ true };
};

class Binds final : public HasLoad
{
private:
	std::vector<Bind> m_binds{};

private:
	void load(const std::string &path);

public:
	void save(const std::string &path);
	bool onLoad() override;

public:
	void add(const Bind bind);
	void remove(Bind *const bind_ptr);

public:
	std::vector<Bind>& getBinds() { return m_binds; }

private:
	void think();
	void paint();

public:
	void run();
};

MAKE_UNIQUE(Binds, binds);