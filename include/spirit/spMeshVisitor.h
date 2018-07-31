#ifndef SP_MESHVISITOR_H
#define SP_MESHVISITOR_H
#include <osg/NodeVisitor>
#include <osg/Geode>
#include <osg/Geometry>
#include <iostream>

class spMeshVisitor : public osg::NodeVisitor
{
public:
    osg::Vec3Array* vertices; // pointer to vetices in .ply file
    osg::Vec3Array* normals; // pointer to normal vectors in .ply file

    spMeshVisitor(): _level(0)
    { setTraversalMode(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN); }
    std::string spaces()
    { return std::string(_level*2, ' '); }

    virtual void apply( osg::Node& node );
    virtual void apply( osg::Geode& geode );
protected:
    unsigned int _level;
};

#endif // SP_MESHVISITOR_H
