
#include "html5.hpp"

html::selector::selector(const std::string& s)
	: m_select_string(s)
{
	build_matchers();
}

html::selector::selector(std::string&&s)
	: m_select_string(s)
{
	build_matchers();
}

template<class GetChar, class GetEscape, class StateEscaper>
static std::string _get_string(GetChar&& getc, GetEscape&& get_escape, char quote_char, StateEscaper&& state_escaper)
{
		std::string ret;

		auto c = getc();

		while ( (c != quote_char) && (c!= '\n'))
		{
			if ( c == '\'' )
			{
				c += get_escape();
			}else
			{
				ret += c;
			}
			c = getc();
		}

		if (c == '\n')
		{
			state_escaper();
		}
		return ret;
}

void html::selector::build_matchers()
{
	// 从 选择字符构建匹配链
	auto str_iterator = m_select_string.begin();

	int state = 0;

	auto getc = [this, &str_iterator]() -> char {
		if (str_iterator != m_select_string.end())
			return *str_iterator++;
		return 0;
	};

	auto get_escape = [&getc]() -> char
	{
		return getc();
	};

	auto get_string = [&getc, &get_escape, &state](char quote_char){
		return _get_string(getc, get_escape, quote_char, [&state](){
			state = 0;
		});
	};

	std::string matcher_str;

	char c;

	do
	{
		c = getc();

#	define METACHAR  0 : case ' ': case '.' : case '#' : case ':': case '['
		switch(state)
		{
			case 0:
				switch(c)
				{
					case '*':
					{
						// 所有的类型
						m_matchers.emplace_back(selector_matcher());
					}
					break;
					case METACHAR:
						state = c;
						break;
					default:
						state = ' ';
						matcher_str += c;
						break;
				}
				break;
			case '#':
			case '.':
			case ' ':
				switch(c)
				{
					case '\\':
						matcher_str += get_escape();
						break;
					case METACHAR:
						{
							selector_matcher matcher;
							switch(state)
							{
								case ' ':
									matcher.matching_tag_name = std::move(matcher_str);
									break;
								case '#':
									matcher.matching_id = std::move(matcher_str);
									break;
								case '.':
									matcher.matching_class = std::move(matcher_str);
									break;
							}
							m_matchers.push_back(std::move(matcher));
							state = c;
						}
						break;
					default:
						matcher_str += c;
				}
				break;
			case ':':
			{
				// 冒号暂时不实现
				switch(c)
				{
					case METACHAR:
						state = c;
						break;
				}

			}break;
			case '[':// 暂时不实现
			{
				switch(c)
				{
					case METACHAR:
						state = c;
						break;
				}
			}
			break;
		}
#	undef METACHAR

	}while(c);
}

html::dom::dom(dom* parent) noexcept
	: m_parent(parent)
{
}

html::dom::dom(const std::string& html_page, dom* parent)
	: dom(parent)
{
	append_partial_html(html_page);
}

html::dom::dom(html::dom&& d)
    : attributes(std::move(d.attributes))
    , tag_name(std::move(d.tag_name))
    , contents(std::move(d.contents))
    , m_parent(std::move(d.m_parent))
	, children(std::move(d.children))
{
}

html::dom::dom(const html::dom& d)
    : attributes(d.attributes)
    , tag_name(d.tag_name)
    , contents(d.contents)
    , m_parent(d.m_parent)
	, children(d.children)
{
}

html::dom& html::dom::operator=(const html::dom& d)
{
    attributes = d.attributes;
    tag_name = d.tag_name;
    contents = d.contents;
    m_parent = d.m_parent;
	children = d.children;
	return *this;
}

bool html::dom::append_partial_html(const std::string& str)
{
	if (!html_parser_feeder_inialized)
	{
		html_parser_feeder = boost::coroutines::asymmetric_coroutine<char>::push_type(std::bind(&dom::html_parser, this, std::placeholders::_1));
		html_parser_feeder_inialized = true;
	}

	for(auto c: str)
		html_parser_feeder(c);
	return true;
}

template<class Handler>
void html::dom::dom_walk(html::dom_ptr d, Handler handler)
{
	if(handler(d))
		for (auto & c : d->children)
			dom_walk(c, handler);
}

bool html::selector::selector_matcher::operator()(const html::dom& d) const
{
	if (!matching_tag_name.empty())
	{
		return d.tag_name == matching_tag_name;
	}
	if (!matching_id.empty())
	{
		auto it = d.attributes.find("id");
		if ( it != d.attributes.end())
		{
			return it->second == matching_id;
		}
	}
	return false;
}

html::dom html::dom::operator[](const selector& selector_)
{
	html::dom selectee_dom(*this);
	html::dom matched_dom;

	for (auto & matcher : selector_)
	{
		for( auto & c : selectee_dom.children)
		{
			dom_walk(c, [this, &matcher, &matched_dom, selector_](html::dom_ptr i)
			{
				bool no_match = true;

				dom* _this_dom = i.get();

				std::string id = i->attributes["id"];

				if (matcher(*i))
				{
					no_match = false;
				}else
					no_match = true;

				if (!no_match)
					matched_dom.children.push_back(i);
				return no_match;
			});
		}
		selectee_dom = matched_dom;
	}

	return matched_dom;
}

std::string html::dom::to_plain_text()
{


	std::string ret;
	return ret;
}


#define CASE_BLANK case ' ': case '\r': case '\n': case '\t'

void html::dom::html_parser(boost::coroutines::asymmetric_coroutine<char>::pull_type& html_page_source)
{
	int pre_state = 0, state = 0;

	auto getc = [&html_page_source](){
		auto c = html_page_source.get();
		html_page_source();
		return c;
	};

	auto get_escape = [&getc]() -> char
	{
		return getc();
	};

	auto get_string = [&getc, &get_escape, &pre_state, &state](char quote_char){
		return _get_string(getc, get_escape, quote_char, [&pre_state, &state](){
			pre_state = state;
			state = 0;
		});
	};

	std::string tag; //当前处理的 tag
	std::string content; // 当前 tag 下的内容
	std::string k,v;

	dom * current_ptr = this;

	char c;

	bool ignore_blank = true;
	std::vector<int> comment_stack;

	while(html_page_source) // EOF 检测
	{
		// 获取一个字符
		c = getc();

		switch(state)
		{
			case 0: // 起始状态. content 状态
			{
				switch(c)
				{
					case '<':
					{
						if (tag.empty())
						{
							// 进入 < tag 解析状态
							pre_state = state;
							state = 1;
							if (!content.empty())
								current_ptr->contents.push_back(std::move(content));
							ignore_blank = (tag != "script");
						}
					}
					break;
					CASE_BLANK :
					if(ignore_blank)
						break;
					default:
						content += c;
				}
			}
			break;
			case 1: // tag名字解析
			{
				switch (c)
				{
					CASE_BLANK :
					{
						if (tag.empty())
						{
							// empty tag name
							// 重新进入上一个 state
							// 也就是忽略 tag
							state = pre_state;
						}else
						{
							pre_state = state;
							state = 2;

							dom_ptr new_dom = std::make_shared<dom>(current_ptr);
							new_dom->tag_name = std::move(tag);

							current_ptr->children.push_back(new_dom);
							if(new_dom->tag_name[0] != '!')
								current_ptr = new_dom.get();
						}
					}
					break;
					case '>': // tag 解析完毕, 正式进入 下一个 tag
					{
						pre_state = state;
						state = 0;

						dom_ptr new_dom = std::make_shared<dom>(current_ptr);
						new_dom->tag_name = std::move(tag);
						current_ptr->children.push_back(new_dom);
						if(new_dom->tag_name[0] != '!')
							current_ptr = new_dom.get();
					}
					break;
					case '/':
						pre_state = state;
						state = 5;
					break;
					case '!':
						pre_state = state;
						state = 10;
					// 为 tag 赋值.
					default:
						tag += c;
				}
			}
			break;
			case 2: // tag 名字解析完毕, 进入 attribute 解析, skiping white
			{
				switch (c)
				{
					CASE_BLANK :
					break;
					case '>': // 马上就关闭 tag 了啊
					{
						// tag 解析完毕, 正式进入 下一个 tag
						pre_state = state;
						state = 0;
						if ( current_ptr->tag_name[0] == '!')
						{
							current_ptr = current_ptr->m_parent;
						}
					}break;
					case '/':
					{
						// 直接关闭本 tag 了
						// 下一个必须是 '>'
						c = getc();

						if (c!= '>')
						{
							// TODO 报告错误
						}

						pre_state = state;
						state = 0;

						if (current_ptr->m_parent)
							current_ptr = current_ptr->m_parent;
						else
							current_ptr = this;
					}break;
					case '\"':
					case '\'':
					{
						pre_state = state;
						state = 3;
						k = get_string(c);
					}break;
					default:
						pre_state = state;
						state = 3;
						k += c;
				}
			}break;
			case 3: // tag 名字解析完毕, 进入 attribute 解析 key
			{
				switch (c)
				{
					CASE_BLANK :
					{
						// empty k=v
						state = 2;
						current_ptr->attributes[k] = "";
						k.clear();
						v.clear();
					}
					break;
					case '=':
					{
						pre_state = state;
						state = 4;
					}break;
					case '>':
					{
						pre_state = state;
						state = 0;
						current_ptr->attributes[k] = "";
						k.clear();
						v.clear();
					}
					break;
					default:
						k += c;
				}
			}break;
			case 4: // 进入 attribute 解析 value
			{
				switch (c)
				{
					case '\"':
					case '\'':
					{
						v = get_string(c);
					}
					CASE_BLANK :
					{
						state = 2;
						current_ptr->attributes[k] = std::move(v);
						k.clear();
					}
					break;
					default:
						v += c;
				}
			}
			break;
			case 5:
			{
				switch(c)
				{
					case '>':
					{

						if(!tag.empty())
						{
							// 来, 关闭 tag 了
							// 注意, HTML 里, tag 可以越级关闭

							// 因此需要进行回朔查找

							auto _current_ptr = current_ptr;

							while (_current_ptr && _current_ptr->tag_name != tag)
							{
								_current_ptr = _current_ptr->m_parent;
							}

							tag.clear();
							if (!_current_ptr)
							{
								// 找不到对应的 tag 要咋关闭... 忽略之
								break;
							}

							current_ptr = _current_ptr;

							// 找到了要关闭的 tag

							// 那就退出本 dom 节点
							if (current_ptr->m_parent)
								current_ptr = current_ptr->m_parent;
							else
								current_ptr = this;
							state = 0;
						}
					}break;
					CASE_BLANK:
					break;
					default:
					{
						// 这个时候需要吃到  >
						tag += c;
					}
				}
				break;
			}

			case 10:
			{
				// 遇到了 <! 这种,
				switch(c)
				{
					case '-':
						tag += c;
						state = 11;
						break;
					default:
						tag += c;
						state = pre_state;
				}
			}break;
			case 11:
			{
				// 遇到了 <!- 这种,
				switch(c)
				{
					case '-':
						state = 12;
						comment_stack.push_back(pre_state);
						tag.clear();
						break;
					default:
						tag += c;
						state = pre_state;
				}

			}break;
			case 12:
			{
				// 遇到了 <!-- 这种,
				// 要做的就是一直忽略直到  -->
				switch(c)
				{
					case '<':
					{
						c = getc();
						if ( c == '!')
						{
							pre_state = state;
							state = 10;
						}else{
							content += '<';
							content += c;
						}
					}break;
					case '-':
					{
						pre_state = state;
						state = 13;
					}
					default:
						content += c;
				}
			}break;
			case 13: // 遇到 -->
			{
				switch(c)
				{
					case '-':
					{
						state = 14;
					}break;
					default:
						content += c;
						state = pre_state;
				}
			}break;
			case 14: // 遇到 -->
			{
				switch(c)
				{
					case '>':
					{
						if (pre_state == 12)
							content.pop_back();
						comment_stack.pop_back();
						state = comment_stack.empty()? 0 : 12;
						dom_ptr comment_node = std::make_shared<dom>(current_ptr);
						comment_node->tag_name = "<!--";
						comment_node->contents.push_back(std::move(content));
						current_ptr->children.push_back(comment_node);
					}break;
					default:
						content += c;
						state = pre_state;
				}
			}break;
		}
	}
}

#undef CASE_BLANK


