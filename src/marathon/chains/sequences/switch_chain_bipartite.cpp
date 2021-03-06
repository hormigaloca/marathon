// project includes
#include "../../../../include/marathon/chains/sequences/switch_chain_bipartite.h"
#include "../../../../include/marathon/chains/sequences/havel_hakimi.h"

//#define DEBUG

namespace marathon {

namespace chain {

namespace sequence {

SwitchBipartite::SwitchBipartite(const std::string& line) :
		sum(0) {

	std::string copy(line);

	// split string at ";"
	std::vector<std::string> vec;

	std::string delimiter = ";";
	size_t pos = 0;
	std::string token;
	while ((pos = copy.find(delimiter)) != std::string::npos) {
		token = copy.substr(0, pos);
		vec.push_back(token);
		copy.erase(0, pos + delimiter.length());
	}
	vec.push_back(copy);

	if (vec.size() != 2) {
		std::cerr << "invalid syntax: " << line << std::endl;
		return;
	}

	delimiter = ",";
	pos = 0;

	copy = vec[0];
	while ((pos = copy.find(delimiter)) != std::string::npos) {
		token = copy.substr(0, pos);
		u.push_back(atoi(token.c_str()));
		copy.erase(0, pos + delimiter.length());
	}
	u.push_back(atoi(copy.c_str()));

	copy = vec[1];
	while ((pos = copy.find(delimiter)) != std::string::npos) {
		token = copy.substr(0, pos);
		v.push_back(atoi(token.c_str()));
		copy.erase(0, pos + delimiter.length());
	}
	v.push_back(atoi(copy.c_str()));

	for (auto it = v.begin(); it != v.end(); ++it)
		sum += *it;

#ifdef DEBUG
	std::cout << "u=[";
	for (std::vector<int>::iterator it = u.begin(); it != u.end(); ++it) {
		std::cout << *it << " ";
	}
	std::cout << "]" << std::endl;

	std::cout << "v=[";
	for (std::vector<int>::iterator it = v.begin(); it != v.end(); ++it) {
		std::cout << *it << " ";
	}
	std::cout << "]" << std::endl;
#endif

}

bool SwitchBipartite::computeArbitraryState(DenseBipartiteGraph& s) const {

	// not a valid instance
	if (u.size() < 2 || v.size() < 2) {
		return false;
	}

	bool M[u.size() * v.size()];
	if (HavelHakimiBipartite(u, v, M) != 0) {
		s = DenseBipartiteGraph(u.size(), v.size(), M);
		return true;
	} else
		return false;
}

void SwitchBipartite::computeNeighbours(const DenseBipartiteGraph& s,
		boost::unordered_map<DenseBipartiteGraph, Rational>& neighbours) const {
	int i, j, k, l, m, n;
	DenseBipartiteGraph s2;

	m = s.get_nrows();
	n = s.get_ncols();

	// Definition of Kannan, Tetali, Vempala
	for (i = 0; i < m; i++) {
		for (j = 0; j < n; j++) {
			for (k = i; k < m; k++) {
				for (l = j; l < n; l++) {

					s2 = DenseBipartiteGraph(s);

					// alternating cycle ( (i,j)=1, (k,j)=0, (k,l)=1, (i,l)=0 )
					if (s.has_edge(i, j) && s.has_edge(k, l)
							&& !s.has_edge(i, l) && !s.has_edge(k, j)) {
						// switch the cycle
						s2.flip_edge(i, j); // (i,j) = 0
						s2.flip_edge(k, l); // (k,l) = 0
						s2.flip_edge(i, l); // (i,l) = 1
						s2.flip_edge(k, j); // (k,j) = 1
					}
					// symmetric case (attention: not regarded in paper!)
					else if (!s.has_edge(i, j) && !s.has_edge(k, l)
							&& s.has_edge(i, l) && s.has_edge(k, j)) {
						// switch the cycle
						s2.flip_edge(i, j); // (i,j) = 1
						s2.flip_edge(k, l); // (k,l) = 1
						s2.flip_edge(i, l); // (i,l) = 0
						s2.flip_edge(k, j); // (k,j) = 0
					}

					// each state has proposal prob. of 4 / (m*(m+1)*n*(n+1))
					neighbours[s2] += Rational(4, m * (m + 1) * n * (n + 1));
				}
			}
		}
	}
}

int SwitchBipartite::next_red_edge(int col, bool* red_edges, int m,
		int n) const {
	for (int i = 0; i < m; i++) {
		if (red_edges[i * n + col])
			return i;
	}
	// no edge found in column
	return -1;
}

int SwitchBipartite::next_blue_edge(int row, bool* blue_edges, int m,
		int n) const {
	for (int j = 0; j < n; j++) {
		if (blue_edges[row * n + j])
			return j;
	}
	// no blue edge found in row
	return -1;
}

void SwitchBipartite::trace_cycle(bool* blue_edges, bool* red_edges, int m,
		int n, int i, int j, std::vector<int>& cycle) const {

	while (j != -1) {

		// add (i,j) to cycle
		cycle.push_back(i);
		cycle.push_back(m + j);

		// remove blue edge (i,j)
		blue_edges[i * n + j] = false;

		i = next_red_edge(j, red_edges, m, n);

		// remove red edge (i,j)
		red_edges[i * n + j] = false;

		j = next_blue_edge(i, blue_edges, m, n);
	};
}

void SwitchBipartite::splice_cycle(std::vector<int> cycle,
		std::list<std::vector<int> >& cycles) const {

#ifdef DEBUG
	std::cout << "splice cycle [ ";
	for (std::vector<int>::iterator it = cycle.begin(); it != cycle.end();
			++it) {
		std::cout << *it << " ";
	}
	std::cout << "]" << std::endl;
#endif

	int m = u.size();
	int n = v.size();

	std::vector<int> c;
	bool removed[cycle.size()];
	int first_seen[n + m];
	int i, j;

	memset(removed, 0, cycle.size() * sizeof(bool));

	for (i = 0; i < n + m; i++)
		first_seen[i] = n + m;

	for (i = 0; i < cycle.size(); i++) {

#ifdef DEBUG
		std::cout << i << ": " << cycle[i] << std::endl;
#endif

		// smaller cycle detected
		if (first_seen[cycle[i]] != n + m) {

#ifdef DEBUG
			std::cout << "smaller cycle detected" << std::endl;
#endif

			// extract smaller cycle and store in cycle list
			c.clear();
			for (j = first_seen[cycle[i]]; j != i; j++) {
				if (!removed[j]) {
					c.push_back(cycle[j]);
					first_seen[cycle[j]] = n + m;
					removed[j] = true;
				}
			}
			cycles.push_back(c);
		}
		first_seen[cycle[i]] = i;
	}

	// not removed vertices
	c.clear();
	for (i = 0; i < cycle.size(); i++) {
		if (!removed[i])
			c.push_back(cycle[i]);
	}
	cycles.push_back(c);
}

void SwitchBipartite::cycle_decomposition(const DenseBipartiteGraph& x,
		const DenseBipartiteGraph& y,
		std::list<std::vector<int> >& cycles) const {

	int m = x.get_nrows();
	int n = x.get_ncols();

	bool red[m * n];
	bool blue[m * n];

	std::vector<int> cycle;
	std::vector<int>::iterator cit;
	std::list<std::vector<int> >::iterator it;

	memset(red, 0, m * n * sizeof(bool));
	memset(blue, 0, m * n * sizeof(bool));

	for (int i = 0; i < m; i++) {
		for (int j = 0; j < n; j++) {
			if (x.has_edge(i, j) && !y.has_edge(i, j))
				blue[COORD_TRANSFORM(i, j, n)] = true;
			else if (!x.has_edge(i, j) && y.has_edge(i, j))
				red[COORD_TRANSFORM(i, j, n)] = true;
		}
	}

	for (int i = 0; i < m; i++) {
		for (int j = 0; j < n; j++) {
			if (blue[COORD_TRANSFORM(i, j, n)]) {
				// start of alternating Cycle in x found
				cycle.clear();
				// trace cycle
				trace_cycle(blue, red, m, n, i, j, cycle);
				// try to splice cycles into smaller ones
				splice_cycle(cycle, cycles);
			}
		}
	}
}

struct SwitchBipartite::cycle_comparator {
	bool operator()(const std::vector<int>& c1, const std::vector<int>& c2) {
		return c1.size() < c2.size();
	}
};

void SwitchBipartite::canonicalPath(int from, int to,
		std::list<int>& path) const {

#ifdef DEBUG
	std::cout << "from=" << states[from] << std::endl;
	std::cout << "to  =" << states[to] << std::endl;
#endif

	// start with from
	path.push_back(from);

	// continously modify u
	DenseBipartiteGraph u = states[from];
	DenseBipartiteGraph v = states[to];

	std::list<std::vector<int> > cycles;
	std::list<std::vector<int> >::iterator it;

	int i, l;
	std::vector<int> w;
	std::vector<int>::iterator it2;

	// decompose symmetric difference of u and v into cycles
	cycle_decomposition(u, v, cycles);
#ifdef DEBUG
	std::cout << "found " << cycles.size() << " cycles" << std::endl;
#endif

	// sort cycles by length
	cycles.sort(cycle_comparator());

	while (!cycles.empty()) {

		// deal with smallest cycle
		w = cycles.front();
		cycles.pop_front();

		assert(w.size() % 2 == 0);
		assert(w.size() >= 4);
#ifdef DEBUG
		std::cout << "[";
		for (it2 = w.begin(); it2 != w.end(); ++it2) {
			std::cout << " " << *it2;
		}
		std::cout << " ]" << std::endl;
#endif

		l = w.size();

		/*
		 * find first vertex w[i] s.t. the cycle
		 *     (w[0], w[1], ..., w[i], w[i+1], w[l-i-2], w[l-i-1], ... , w[l-1])
		 * is alternating in u
		 */
		i = 0;
		while (!u.is_switchable(w[i], w[i + 1], w[l - i - 2], w[l - i - 1])) {
			i++;
		}

#ifdef DEBUG
		std::cout << "switch cycle: [ " << w[i] << " " << w[i + 1] << " "
		<< w[l - i - 2] << " " << w[l - i - 1] << " ]" << std::endl;
#endif

		/**
		 * (w[i], w[i+1], w[l-i-2], w[l-i-1]) is switchable
		 *    switch it!
		 */
		u.switch_4_cycle(w[i], w[i + 1], w[l - i - 2], w[l - i - 1]);
		int x = indices.find(u)->second;
		path.push_back(x);

		/**
		 * two smaller alternating cycles are created:
		 *  1: (w[0], ... , w[i], w[l-i-1], ... , w[l-1])
		 *  2: (w[i+1], ... , w[l-i-2])
		 *
		 *  Erase first cycle by series of switches
		 */

		for (int j = i; j > 0; j--) {
			u.switch_4_cycle(w[j - 1], w[j], w[l - j - 1], w[l - j]);
			int x = indices.find(u)->second;
			path.push_back(x);
		}

		/**
		 * We are left with the second cycle which is smaller
		 * Continue with this cycle
		 */

		// if second cycle has at least 4 edges
		if (l - 2 * i - 2 >= 4) {
			w.clear();
			for (int j = i + 1; j <= l - i - 2; j++) {
				w.push_back(w[j]);
			}
			cycles.push_front(w);
		}
	}
}

}

}

}
