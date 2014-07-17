/*
 * QuadNode.h
 *
 *  Created on: 21.05.2014
 *      Author: Moritz v. Looz (moritz.looz-corswarem@kit.edu)
 */

#ifndef QUADNODE_H_
#define QUADNODE_H_

#include <vector>
#include <algorithm>
#include <assert.h>
#include "../../auxiliary/Log.h"
#include "../HyperbolicSpace.h"

using std::vector;
using std::min;
using std::cos;

namespace NetworKit {

template <class T>
class QuadNode {
public:
	QuadNode() {
		leftAngle = 0;
		rightAngle = 2*M_PI;
		minR = 0;
		maxR = 1;//TODO: magic Number, careful.
		capacity = 20;
		isLeaf = true;
		minRegion = 0;
		elements = 0;
	}

	~QuadNode() {
		// TODO Auto-generated constructor stub
	}

	QuadNode(double leftAngle, double minR, double rightAngle, double maxR, unsigned capacity, double minDiameter) {
		this->leftAngle = leftAngle;
		this->minR = minR;
		this->maxR = maxR;
		this->rightAngle = rightAngle;
		this->a = HyperbolicSpace::polarToCartesian(leftAngle, minR);
		this->b = HyperbolicSpace::polarToCartesian(rightAngle, minR);
		this->c = HyperbolicSpace::polarToCartesian(rightAngle, maxR);
		this->d = HyperbolicSpace::polarToCartesian(leftAngle, maxR);
		this->capacity = 20;
		this->minRegion = minDiameter;
		isLeaf = true;
		elements = 0;
	}

	void addContent(T input, double angle, double R) {
		assert(this->responsible(angle, R));
		if (isLeaf) {
			if (content.size() + 1 < capacity ||  HyperbolicSpace::getHyperbolicDistance(leftAngle, minR, rightAngle, maxR) < minRegion) {
				content.push_back(input);
				angles.push_back(angle);
				radii.push_back(R);
				Point<double> pos = HyperbolicSpace::polarToCartesian(angle, R);
				positions.push_back(pos);
			} else {
				//heavy lifting: split up!
				double middleAngle = (rightAngle - leftAngle) / 2 + leftAngle;
				/**
				 * we want to make sure the space is evenly divided to obtain a balanced tree
				 * Simply halving the radius will cause a larger space for the outer Quadnode, resulting in an unbalanced tree
				 */

				double nom = maxR - minR;
				double denom = pow((1-maxR*maxR)/(1-minR*minR), 0.5)+1;
				double middleR = nom/denom + minR;

				QuadNode southwest(leftAngle, minR, middleAngle, middleR, capacity, minRegion);
				QuadNode southeast(middleAngle, minR, rightAngle, middleR, capacity, minRegion);
				QuadNode northwest(leftAngle, middleR, middleAngle, maxR, capacity, minRegion);
				QuadNode northeast(middleAngle, middleR, rightAngle, maxR, capacity, minRegion);
				children = {southwest, southeast, northwest, northeast};

				isLeaf = false;
				for (uint i = 0; i < content.size(); i++) {
					this->addContent(content[i], angles[i], radii[i]);
				}
				content.clear();
				this->addContent(input, angle, R);
			}
		}
		else {
			assert(children.size() > 0);
			//bool foundResponsibleChild = false;
			for (uint i = 0; i < children.size(); i++) {
				if (children[i].responsible(angle, R)) {
					//assert(!foundResponsibleChild);//only one!
					children[i].addContent(input, angle, R);
				//	foundResponsibleChild = true;
				} else {
					//cout << "Not responsible for (" << angle << ", " << R << "). Borders are " << children[i].leftAngle << "-" << children[i].rightAngle << ", and " << children[i].minR << "-" << children[i].maxR << endl;
				}
			}
			//assert(foundResponsibleChild);
		}
		elements++;
	}

	double euclideanLowerBound(Point<double> query) {
		double phi, r;
		HyperbolicSpace::cartesianToPolar(query, phi, r);
		if (responsible(phi, r)) return 0;
		//get four edge points
		double topDistance, bottomDistance, leftDistance, rightDistance;
		if (phi < leftAngle || phi > rightAngle) {
			topDistance = min(c.distance(query), d.distance(query));
		} else {
			topDistance = abs(r - maxR);
		}
		if (phi < leftAngle || phi > rightAngle) {
			bottomDistance = min(a.distance(query), b.distance(query));
		} else {
			bottomDistance = abs(r - minR);
		}

		double minDistanceR = r*cos(abs(phi-leftAngle));
		if (minDistanceR > minR && minDistanceR < maxR) {
			leftDistance = query.distance(HyperbolicSpace::polarToCartesian(phi, minDistanceR));
		} else {
			leftDistance = min(a.distance(query), d.distance(query));
		}

		minDistanceR = r*cos(abs(phi-rightAngle));
		if (minDistanceR > minR && minDistanceR < maxR) {
			rightDistance = query.distance(HyperbolicSpace::polarToCartesian(phi, minDistanceR));
		} else {
			rightDistance = min(b.distance(query), c.distance(query));
		}

		//TRACE("leftDistance:", leftDistance);
		//TRACE("rightDistance:", rightDistance);
		//TRACE("topDistance:", topDistance);
		//TRACE("bottomDistance:", bottomDistance);
		return std::min(std::min(leftDistance, rightDistance), std::min(bottomDistance, topDistance));
	}

	double euclideanLowerBound(double angle, double R) {
		if (responsible(angle, R)) return 0;
		Point<double> query = HyperbolicSpace::polarToCartesian(angle, R);
		return euclideanLowerBound(query);
	}

	bool responsible(double angle, double R) {

		return (angle >= leftAngle && angle < rightAngle && R >= minR && R < maxR);
	}

	std::vector<T> getElements() {
		if (isLeaf) {
			return content;
		} else {
			vector<T> result;
			for (uint i = 0; i < children.size(); i++) {
				std::vector<T> subresult = children[i].getElements();
				result.insert(result.end(), subresult.begin(), subresult.end());
			}
			return result;
		}
	}

	std::vector<T> getCloseElements(Point<double> query, double maxDistance) {
		assert(query.length() < 1);
		std::vector<T> result;
		if (isLeaf) {
			if (this->euclideanLowerBound(query) < maxDistance) {
					for (uint i = 0; i < content.size(); i++) {
						if (HyperbolicSpace::getHyperbolicDistance(query, positions[i]) < maxDistance) {
								result.push_back(content[i]);
							}
					}
			}
		} else {
			for (uint i = 0; i < children.size(); i++) {
				QuadNode * child = &children[i];
				if (child->elements > 0 && child->euclideanLowerBound(query) < maxDistance) {
					vector<T> subresult = child->getCloseElements(query, maxDistance);
					result.insert(result.end(), subresult.begin(), subresult.end());
				}
			}
		}
		return result;
	}

	QuadNode<T> * getAppropriateLeaf(double angle, double R) {
		assert(this->responsible(angle, R));
		if (isLeaf) return this;
		else {
			for (uint i = 0; i < children.size(); i++) {
				bool foundResponsibleChild = false;
				if (children[i].responsible(angle, R)) {
					assert(foundResponsibleChild == false);
					foundResponsibleChild = true;
					return children[i].getAppropriateLeaf(angle, R);
				}
			}
			DEBUG("No responsible child for (", angle, ", ", R, ") found. Segfault imminent.");
		}
	}

	void getElementsInEuclideanCircle(double minAngle, double maxAngle, double lowR, double highR, Point<double> center, double radius, vector<T> &result) {
		if (minAngle >= rightAngle || maxAngle <= leftAngle || lowR >= maxR || highR <= minR) return;
		if (euclideanLowerBound(center) > radius) {
			return;
		}

		double rsq = radius*radius;

		if (isLeaf) {
			for (uint i = 0; i < content.size(); i++) {
				double asq = positions[i][0] - center[0];
				double bsq = positions[i][1] - center[1];
				if (asq*asq + bsq*bsq < rsq) {//maybe improve this with functors
					result.push_back(content[i]);
				}
			}
		}	else {
			for (uint i = 0; i < children.size(); i++) {
				children[i].getElementsInEuclideanCircle(minAngle, maxAngle, lowR, highR, center, radius, result);
			}
		}
	}

	double getLeftAngle() {
		return leftAngle;
	}

	double getRightAngle() {
		return rightAngle;
	}

	double getMinR() {
		return minR;
	}

	double getMaxR() {
		return maxR;
	}

private:
	double leftAngle;
	double rightAngle;
	double minR;
	double maxR;
	Point<double> a,b,c,d;
	unsigned capacity;
	double minRegion;//the minimal region a QuadNode should cover. If it is smaller, don't bother splitting up.
	count elements;
	std::vector<QuadNode> children;
	std::vector<T> content;
	std::vector<Point<double> > positions;
	std::vector<double> angles;
	std::vector<double> radii;
	bool isLeaf;
};
}

#endif /* QUADNODE_H_ */
