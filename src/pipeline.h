#ifndef COMP6771_PIPELINE_H
#define COMP6771_PIPELINE_H

#include <unordered_map>
#include <cstddef>
#include <exception>
#include <limits>
#include <memory>
#include <string>
#include <vector>
namespace ppl {

	// 3.1 pipeline_error
	enum class pipeline_error_kind {
		invalid_node_id,
		no_such_slot,
		slot_already_used,
		connection_type_mismatch,
	};

	struct pipeline_error : std::exception {
		explicit pipeline_error(pipeline_error_kind);
		auto kind() -> pipeline_error_kind;
		auto what() const noexcept -> const char* override;
	};

	// 3.2 node
	enum class poll {
		// A value is available.
		ready,
		// No value is available this time, but there might be one later.
		empty,
		// No value is available, and there never will be again:
		// every future poll for this node will return `poll::closed` again.
		closed,

	};

	class node {
	 public:
		virtual ~node() = default;

		// Returns a human-readable name for the node.
		virtual std::string name() const = 0;

	 private:
		// Process a single tick, preparing the next value.
		virtual poll poll_next() = 0;

		// Connect source as the input to the given slot.
		virtual void connect(const node* source, int slot) = 0;

		// Additional virtual functions can be added here as needed.

		// Grant pipeline class access to private members of node class.
		friend class pipeline;
	};

	// 3.3 producer
	template<typename Output>
	struct producer : node {
		using output_type = Output;
		virtual auto value() const -> const output_type&;
		// only when `Output` is not `void`
	};

	// Specialization for void
	template<>
	struct producer<void> : node {
		using output_type = void;
		// The value() function does not exist
	};

	// 3.4 component
	template<typename Input, typename Output>
	struct component : producer<Output> {
		using input_type = Input;
	};

	// 3.5 sink & source
	template<typename Input>
	struct sink : component<std::tuple<Input>, void> {};

	template<typename Output>
	struct source : component<std::tuple<>, Output> {
	 private:
		void connect(const node* source, int slot) override;
	};

	// 3.6 pipeline
	template<typename N>
	// 3.6.0
	concept concrete_node = requires(N n) {
		                        // Inherits from the node and the producer types
		                        std::is_base_of_v<ppl::node, N>;
		                        std::is_base_of_v<ppl::producer<typename N::output_type>, N>;

		                        // Has a public member type called input_type
		                        typename N::input_type;

		                        // Has a std::tuple input_type
		                        requires std::is_same_v<decltype(std::tuple{std::declval<typename N::input_type>()}),
		                                                std::tuple<typename N::input_type>>;

		                        // Is not an abstract class
		                        !std::is_abstract_v<N>;

		                        // Can be constructed
		                        std::is_constructible_v<N>;
	                        };

	class pipeline {
	 public:
		// 3.6.1

		// define node_id as an opaque handle using a using alias for a std::size_t
		using node_id = std::size_t;

		// a global constant function invalid_node_id returns an "invalid" node_id
		constexpr node_id invalid_node_id() {
			return std::numeric_limits<node_id>::max();
		}

		// is_valid function checks if a node_id is valid or not
		bool is_valid(const node_id& id) {
			return id != invalid_node_id();
		}

		// make_node_id function creates a valid node_id with a given value
		node_id make_node_id(std::size_t value) {
			return value;
		}

		// 3.6.2
		pipeline() = default;
		pipeline(const pipeline&) = delete;
		pipeline(pipeline&&);
		auto operator=(const pipeline&) -> pipeline& = delete;
		auto operator=(pipeline&&) -> pipeline&;
		~pipeline() = default;

		// 3.6.3

		template<typename T>
		struct is_node : std::is_base_of<ppl::node, T> {};

		template<typename N, typename... Args>
		    requires is_node<N>::value
		             && std::is_constructible_v<N, Args...>
		             auto create_node(Args&&... args) -> node_id {
			std::unique_ptr<N> new_node = std::make_unique<N>(std::forward<Args>(args)...);
			node_id id = make_node_id(nodes.size());
			nodes.push_back(std::move(new_node));
			return id;
		}

		void erase_node(node_id n_id) {
			if (is_valid(n_id)) {
				nodes[n_id].reset();
			}
		}

		auto get_node(node_id n_id) -> ppl::node* {
			if (is_valid(n_id)) {
				return nodes[n_id].get();
			}
			return nullptr;
		}

		// 3.6.4
		void connect(node_id src, node_id dst, int slot) {
			// Implement the connection logic here
			ppl::node* src_node = get_node(src);
			ppl::node* dst_node = get_node(dst);

			if (src_node && dst_node) {
				dst_node->connect(src_node, slot);
			}
			else {
				throw ppl::pipeline_error(ppl::pipeline_error_kind::invalid_node_id);
			}
		}

		// 3.6.5
		auto is_valid() -> bool;
		auto step() -> bool;
		void run() {
			while (!step()) {
			}
		}

		// 3.6.6
		friend std::ostream& operator<<(std::ostream&, const pipeline&);

	 private:
		std::vector<std::unique_ptr<node>> nodes;
		std::unordered_map<const node*, int> node_ids;
		// A structure to represent a connection between nodes
		struct connection {
			const node* src;
			const node* dst;
			int slot;
		};
		std::vector<connection> connections;
	};

	inline std::ostream& operator<<(std::ostream& os, const pipeline& p) {
		os << "digraph G {\n";

		for (const auto& n : p.nodes) {
			os << "  \"" << p.node_ids.at(n.get()) << " " << n->name() << "\"\n";
		}

		os << "\n";

		for (const auto& connection : p.connections) {
			/ os << "  \"" << p.node_ids.at(connection.src) << " " << connection.src->name() << "\" -> \""
			     << p.node_ids.at(connection.dst) << " " << connection.dst->name() << "\"\n";
		}

		os << "}\n";
		return os;
	}
}; // namespace ppl

#endif // COMP6771_PIPELINE_H