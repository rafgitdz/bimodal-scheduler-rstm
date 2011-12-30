///////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006, 2007
// University of Rochester
// Department of Computer Science
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the University of Rochester nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

/*  edge.cc
 *
 *  Edges encapsulate the bulk of the information about the triangulation.
 *  Each edge contains references to its endpoints and to the next
 *  edges clockwise and counterclockwise about those endpoints.
 */

#include "config.h"
#include "point.h"
#include "edge.h"
#include "edge_set.h"

// Edge may not be Delaunay.  See if it's the
// diagonal of a convex quadrilateral.  If so, check
// whether it should be flipped.  If so, return         //    c     |
// (through ref params) the edges of the                //   / \    |
// quadrilateral for future reconsideration.  In the    //  a - b   |
// private (nontransactinal) instantiation, do all of   //   \ /    |
// this only if all points I touch are closest to my    //    d     |
// seam: returns false iff called nontransactionally
// and edge cannot safely be reconsidered now.
//
template<class edgeRp, class edgeWp>
    // can be instantiated with
    // <edge::Rp, edge::Wp> or <edge::Up, edge::Up>.
bool reconsider(edgeWp self, const int seam,
                edge::Sp& e1, edge::Sp& e2, edge::Sp& e3, edge::Sp& e4) {
    e1 = 0;  e2 = 0;  e3 = 0;  e4 = 0;
    self->set_tentative(false);
    point* a = self->get_points(0, self.v());
    assert (txnal<edgeRp>() || closest_seam(a) == seam);
    point* b = self->get_points(1, self.v());
    assert (txnal<edgeRp>() || closest_seam(b) == seam);
    edgeRp ac(self->get_neighbors(0, ccw, self.v()));
    edgeRp bc(self->get_neighbors(1, cw, self.v()));
    point* c = ac->get_points(1-index_of(ac, a), ac.v());
    // a and b are assumed to be closest to my seam.
    // I have to check c and d.
    if (!txnal<edgeRp>() && closest_seam(c) != seam) {
        // I can't safely consider this flip in this phase of
        // the algorithm.  Defer to synchronized phase.
        self->set_tentative(true);
        return false;
    }
    if (c != bc->get_points(1-index_of(bc, b), bc.v())) {
        // No triangle on the c side; we're an external edge
        return true;
    }
    edgeRp ad(self->get_neighbors(0, cw, self.v()));
    edgeRp bd(self->get_neighbors(1, ccw, self.v()));
    point* d = ad->get_points(1-index_of(ad, a), ad.v());
    if (!txnal<edgeRp>() && closest_seam(d) != seam) {
        // I can't safely consider this flip in this phase of
        // the algorithm.  Defer to synchronized phase.
        self->set_tentative(true);
        return false;
    }
    if (d != bd->get_points(1-index_of(bd, b), bd.v())) {
        // No triangle on the d side; we're an external edge
        return true;
    }
    if (encircled(b, c, a, d, ccw) || encircled(a, d, b, c, ccw)) {
        // other diagonal is locally Delaunay; we're not
        destroy(self);      // can't wait for delayed destructor
        edgeWp ac_w(ac);  edgeWp ad_w(ad);
        edgeWp bc_w(bc);  edgeWp bd_w(bd);
        edge::Sp dum;
        edge::create(dum, c, d, bc_w, bd_w, cw);
            // Aliasing problem here: since edge constructor modifies
            // neighbors, I can't safely use ac, bc, ad, or bd after this
            // call.  Must use writable versions instead.
        if (!ac_w->get_tentative(ac_w.v())) {
            ac_w->set_tentative(true);  e1 = ac_w;
        }
        if (!ad_w->get_tentative(ad_w.v())) {
            ad_w->set_tentative(true);  e2 = ad_w;
        }
        if (!bc_w->get_tentative(bc_w.v())) {
            bc_w->set_tentative(true);  e3 = bc_w;
        }
        if (!bd_w->get_tentative(bd_w.v())) {
            bd_w->set_tentative(true);  e4 = bd_w;
        }
    }
    return true;
}
// explicit instantiations:
template bool reconsider<edge::Up, edge::Up>
    (edge::Up, const int, edge::Sp&, edge::Sp&, edge::Sp&, edge::Sp&);
template bool reconsider<edge::Rp, edge::Wp>
    (edge::Wp, const int, edge::Sp&, edge::Sp&, edge::Sp&, edge::Sp&);

#ifdef FGL
// Alternative, FGL version: optimistic fine-grain locking.
// Identifies the relevant edges and points, then locks
// them in canonical order, then double-checks to make
// sure nothing has changed.
//
void synchronized_reconsider(edge::Up self, const int seam,
                             simple_queue<edge::Sp> *tentative_edges) {
    self->set_tentative(false);
        // Do this first, to avoid window where I turn it off
        // after somebody else has turned it on.
    point* a = self->get_points(0, self.v());
    point* b = self->get_points(1, self.v());
    edge::Up ac(self->get_neighbors(0, ccw, self.v()));
    edge::Up bc(self->get_neighbors(1, cw, self.v()));
    int aci = index_of(ac, a);
    int bci = index_of(bc, b);
    if (aci == -1 || bci == -1) {
        // inconsistent; somebody has already been here
        return;
    }
    point* c = ac->get_points(1-aci, ac.v());
    if (c != bc->get_points(1-bci, bc.v())) {
        // No triangle on the c side; we're an external edge
        return;
    }
    edge::Up ad(self->get_neighbors(0, cw, self.v()));
    edge::Up bd(self->get_neighbors(1, ccw, self.v()));
    int adi = index_of(ad, a);
    int bdi = index_of(bd, b);
    if (adi == -1 || bdi == -1) {
        // inconsistent; somebody has already been here
        return;
    }
    point* d = ad->get_points(1-adi, ad.v());
    if (d != bd->get_points(1-bdi, bd.v())) {
        // No triangle on the d side; we're an external edge
        return;
    }
    {
        point_set S;
        with_locked_points cs(S | a | b | c | d);

        if (!(edges->contains(self)
                && edges->contains(ac) && edges->contains(bc)
                && edges->contains(ad) && edges->contains(bd))) {
            // inconsistent; somebody has already been here
            return;
        }
        if (!(ac == self->get_neighbors(0, ccw, self.v())
                && bc == self->get_neighbors(1, cw, self.v())
                && ad == self->get_neighbors(0, cw, self.v())
                && bd == self->get_neighbors(1, ccw, self.v()))) {
            // inconsistent; somebody has already been here
            return;
        }
        if (!(aci == index_of(ac, a)
                && bci == index_of(bc, b)
                && adi == index_of(ad, a)
                && bdi == index_of(bd, b))) {
            // inconsistent; somebody has already been here
            return;
        }
        if (!(c == ac->get_points(1-aci, ac.v())
                && c == bc->get_points(1-bci, bc.v())
                && d == ad->get_points(1-adi, ad.v())
                && d == bd->get_points(1-bdi, bd.v()))) {
            // inconsistent; somebody has already been here
            return;
        }
        if (encircled(b, c, a, d, ccw) || encircled(a, d, b, c, ccw)) {
            // other diagonal is locally Delaunay; we're not
            destroy(self);      // can't wait for delayed destructor
            edge::Sp dum;
            edge::create(dum, c, d, bc, bd, cw);
            ac->set_tentative(true);  tentative_edges->enqueue(ac, seam);
            ad->set_tentative(true);  tentative_edges->enqueue(ad, seam);
            bc->set_tentative(true);  tentative_edges->enqueue(bc, seam);
            bd->set_tentative(true);  tentative_edges->enqueue(bd, seam);
        }
    }
}
#endif  // FGL

// Utility routine for constructor.
// can be instantiated with <edge::Wp> or with <edge::Up>
//
template<class edgeWp>
void edge::initialize_end(edge::Sp self, point* p,
                          edgeWp e, int end, int dir) {
    if (e == 0) {
        set_neighbors(end, dir, self);
        set_neighbors(end, 1-dir, self);
        if (txnal<edgeWp>()) {
            p->tx_set_first_edge(self);
        } else {
            p->pv_set_first_edge(self);
        }
    } else {
        int i = index_of(e, p);
        set_neighbors(end, 1-dir, e);
        set_neighbors(end, dir, e->get_neighbors(i, dir, e.v()));
        e->set_neighbors(i, dir, self);
        stm::internal::Validator dv;
            // dummy validator; safe in constructor
        edge::Sp nbor_s(get_neighbors(end, dir, dv));
        edgeWp nbor_w(nbor_s);
        i = index_of(nbor_w, p);
        nbor_w->set_neighbors(i, 1-dir, self);
    }
}

// Edge "constructor": connect points A and B, inserting dir (CW or CCW)
// of edge Ea at the A end and 1-dir of edge Eb at the B end.
// Either or both of Ea and Eb may be null.
// Sets reference parameter self to be edge::Sp(new edge).
// Can be instantiated with <edge::Wp> or with <edge::Up>
//
template<class edgeWp>
void edge::create(edge::Sp& self, point* a, point* b,
                  edgeWp ea, edgeWp eb, int dir) {
    edge* bare_self = new edge();
    self = edge::Sp(bare_self);
    bare_self->set_tentative(false);
    bare_self->set_points(0, a);  bare_self->set_points(1, b);
    bare_self->initialize_end(self, a, ea, 0, dir);
    bare_self->initialize_end(self, b, eb, 1, 1-dir);
    typedef typename edgeWp::template r_analogue<edge>::type edgeRp;
    edgeRp self_r(self);
    edges->insert(self_r);
}

// Edge destructor: take self out of edges, point edge lists.
// Should only be called when flipping an edge, so destroyed
// edge should have neighbors at both ends.
// Caller should hold locks that cover endpoints and neighbors.
//
template<class edgeWp>
void destroy(edgeWp e) {
    edges->erase(e);
    for (int i = 0; i < 2; i++) {
        point* p = e->get_points(i, e.v());
        edgeWp cw_nbor(e->get_neighbors(i, cw, e.v()));
        edgeWp ccw_nbor(e->get_neighbors(i, ccw, e.v()));
        int cw_index = index_of(cw_nbor, p);
        int ccw_index = index_of(ccw_nbor, p);
        cw_nbor->set_neighbors(cw_index, ccw, ccw_nbor);
        ccw_nbor->set_neighbors(ccw_index, cw, cw_nbor);
        if (txnal<edgeWp>()) {
            if (p->tx_get_first_edge() == e)
                p->tx_set_first_edge(ccw_nbor);
        } else {
            if (p->pv_get_first_edge() == e)
                p->pv_set_first_edge(ccw_nbor);
        }
    }
    tx_delete(e);
}
// explicit instantiations:
template void destroy<edge::Up>(edge::Up e);
template void destroy<edge::Wp>(edge::Wp e);
